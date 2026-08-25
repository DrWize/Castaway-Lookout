package main

import (
	"bytes"
	"crypto/md5"
	"encoding/binary"
	"fmt"
	"image"
	"image/color"
	"image/png"
	"math"
	"os"
	"path/filepath"
	"reflect"
	"strings"
	"testing"
	"time"

	rl "github.com/gen2brain/raylib-go/raylib"
)

func TestParseOptions(t *testing.T) {
	tests := []struct {
		name    string
		args    []string
		want    appOptions
		wantErr bool
	}{
		{name: "defaults", want: appOptions{monitor: 1}},
		{name: "Fast CRT override", args: []string{"--crt", "FAST"}, want: appOptions{monitor: 1, crt: "fast"}},
		{name: "HDR Pop override", args: []string{"--crt", "HDR"}, want: appOptions{monitor: 1, crt: "hdr"}},
		{
			name: "all options",
			args: []string{"--windowed", "--screensaver", "--mute", "--stretch", "--monitor", "2", "--ttm", "fire.ttm", "--menu", "--data-dir", "C:\\Johnny"},
			want: appOptions{windowed: true, screenSaver: true, mute: true, stretch: true, monitor: 2, ttm: "FIRE.TTM", menu: true, dataDir: "C:\\Johnny"},
		},
		{name: "invalid monitor", args: []string{"--monitor", "0"}, wantErr: true},
		{name: "invalid CRT mode", args: []string{"--crt", "royale"}, wantErr: true},
		{name: "conflicting window modes", args: []string{"--windowed", "--fullscreen"}, wantErr: true},
		{name: "conflicting audio modes", args: []string{"--mute", "--sound"}, wantErr: true},
		{name: "conflicting aspect modes", args: []string{"--stretch", "--fit"}, wantErr: true},
		{name: "invalid TTM extension", args: []string{"--ttm", "FIRE.BMP"}, wantErr: true},
		{name: "unknown option", args: []string{"--unknown"}, wantErr: true},
	}

	for _, test := range tests {
		t.Run(test.name, func(t *testing.T) {
			got, err := parseOptions(test.args)
			if (err != nil) != test.wantErr {
				t.Fatalf("parseOptions() error = %v, wantErr %t", err, test.wantErr)
			}
			if !test.wantErr && !reflect.DeepEqual(got, test.want) {
				t.Fatalf("parseOptions() = %#v, want %#v", got, test.want)
			}
		})
	}
}

func TestCRTCapabilities(t *testing.T) {
	all := crtCapabilities{fast: true, hdr: true, lottes: true}
	if got, want := all.modes(), []crtFilter{crtOff, crtLightweight, crtFast, crtHDR, crtLottes}; !reflect.DeepEqual(got, want) {
		t.Fatalf("all-capability modes = %#v, want %#v", got, want)
	}
	if got := all.next(crtLottes); got != crtOff {
		t.Fatalf("next(lottes) = %q, want off", got)
	}
	if !crtHDR.usesNativeFrame() {
		t.Fatal("HDR Pop must sample the native completed frame")
	}

	limited := crtCapabilities{}
	if got, want := limited.modes(), []crtFilter{crtOff, crtLightweight}; !reflect.DeepEqual(got, want) {
		t.Fatalf("limited modes = %#v, want %#v", got, want)
	}
	if got := limited.next(crtLightweight); got != crtOff {
		t.Fatalf("limited next(lightweight) = %q, want off", got)
	}
	if got := limited.fallback(crtFast); got != crtLightweight {
		t.Fatalf("fallback(fast) = %q, want lightweight", got)
	}

	fastOnly := crtCapabilities{fast: true}
	if got := fastOnly.next(crtLightweight); got != crtFast {
		t.Fatalf("fast-only next(lightweight) = %q, want fast", got)
	}
	if got := fastOnly.next(crtFast); got != crtOff {
		t.Fatalf("fast-only next(fast) = %q, want off", got)
	}

	hdrOnly := crtCapabilities{hdr: true}
	if got := hdrOnly.next(crtLightweight); got != crtHDR {
		t.Fatalf("HDR-only next(lightweight) = %q, want hdr", got)
	}
	if got := hdrOnly.fallback(crtHDR); got != crtHDR {
		t.Fatalf("HDR fallback = %q, want hdr", got)
	}
}

func TestNormalizeLayerClip(t *testing.T) {
	tests := []struct {
		name           string
		x1, y1, x2, y2 int32
		want           layerClip
	}{
		{name: "normal", x1: 10, y1: 20, x2: 110, y2: 220, want: layerClip{x: 10, y: 20, width: 100, height: 200}},
		{name: "canvas bounds", x1: -20, y1: -10, x2: 700, y2: 500, want: layerClip{x: 0, y: 0, width: 640, height: 480}},
		{name: "empty reversed", x1: 80, y1: 90, x2: 20, y2: 30, want: layerClip{x: 80, y: 90}},
	}
	for _, test := range tests {
		t.Run(test.name, func(t *testing.T) {
			if got := normalizeLayerClip(test.x1, test.y1, test.x2, test.y2); got != test.want {
				t.Fatalf("normalizeLayerClip() = %#v, want %#v", got, test.want)
			}
		})
	}
}

func TestCloudPlacementLimitsMatchReferenceCanvas(t *testing.T) {
	tests := []struct {
		cloudNo    int
		maxX, maxY int
	}{
		{cloudNo: 0, maxX: 511, maxY: 99},
		{cloudNo: 1, maxX: 448, maxY: 78},
		{cloudNo: 2, maxX: 376, maxY: 59},
	}
	for _, test := range tests {
		gotX, gotY := cloudPlacementLimits(test.cloudNo)
		if gotX != test.maxX || gotY != test.maxY {
			t.Fatalf("cloudPlacementLimits(%d) = (%d, %d), want (%d, %d)", test.cloudNo, gotX, gotY, test.maxX, test.maxY)
		}
	}
}

func TestCloudNextXMovesAndWraps(t *testing.T) {
	tests := []struct {
		name                string
		x, speed, direction int32
		want                int32
	}{
		{name: "right", x: 100, speed: 2, direction: 0, want: 102},
		{name: "left", x: 100, speed: 1, direction: 1, want: 99},
		{name: "wrap right edge", x: 905, speed: 2, direction: 0, want: -264},
		{name: "wrap left edge", x: -265, speed: 1, direction: 1, want: 904},
	}
	for _, test := range tests {
		if got := cloudNextX(test.x, test.speed, test.direction); got != test.want {
			t.Errorf("%s: cloudNextX() = %d, want %d", test.name, got, test.want)
		}
	}
}

func TestStoryNightForHour(t *testing.T) {
	tests := []struct {
		hour, override, want int
	}{
		{hour: 0, override: -1, want: 1},
		{hour: 5, override: -1, want: 1},
		{hour: 6, override: -1, want: 0},
		{hour: 17, override: -1, want: 0},
		{hour: 18, override: -1, want: 1},
		{hour: 23, override: -1, want: 1},
		{hour: 23, override: 0, want: 0},
		{hour: 12, override: 1, want: 1},
	}
	for _, test := range tests {
		if got := storyNightForHour(test.hour, test.override); got != test.want {
			t.Errorf("storyNightForHour(%d, %d) = %d, want %d", test.hour, test.override, got, test.want)
		}
	}
}

func TestDayNightKeyboardAndSettingsCycle(t *testing.T) {
	oldOverride := islandDayNightOverride
	oldNight := islandState.night
	t.Cleanup(func() {
		islandDayNightOverride = oldOverride
		islandState.night = oldNight
	})
	islandDayNightOverride = -1

	if got := islandCycleDayNight(); got != "Day" {
		t.Fatalf("first day/night preview = %q, want Day", got)
	}
	if got := islandCycleDayNight(); got != "Night" {
		t.Fatalf("second day/night preview = %q, want Night", got)
	}
	if got := islandCycleDayNight(); got != "Automatic (clock)" {
		t.Fatalf("third day/night preview = %q, want Automatic (clock)", got)
	}
}

func TestStandaloneIslandScreenFollowsDayNightOverride(t *testing.T) {
	oldContent := currentContent
	oldNight := islandState.night
	oldScreen := standaloneDayScreenName
	t.Cleanup(func() {
		currentContent = oldContent
		islandState.night = oldNight
		standaloneDayScreenName = oldScreen
	})

	currentContent = "GJGULIVR.TTM"
	islandState.night = 1
	if got := islandStandaloneScreen("OCEAN01.SCR"); got != "NIGHT.SCR" {
		t.Fatalf("night standalone screen = %q, want NIGHT.SCR", got)
	}
	if standaloneDayScreenName != "OCEAN01.SCR" {
		t.Fatalf("remembered day screen = %q, want OCEAN01.SCR", standaloneDayScreenName)
	}
	if got := islandStandaloneScreen("ISLETEMP.SCR"); got != "NIGHT.SCR" {
		t.Fatalf("Lilliput night screen = %q, want NIGHT.SCR", got)
	}

	islandState.night = 0
	if got := islandStandaloneScreen("OCEAN02.SCR"); got != "OCEAN02.SCR" {
		t.Fatalf("day standalone screen = %q, want OCEAN02.SCR", got)
	}
	if got := islandStandaloneScreen("CITY.SCR"); got != "CITY.SCR" {
		t.Fatalf("non-island screen = %q, want unchanged CITY.SCR", got)
	}
}

func TestDayNightShortcutUsesCapturedKey(t *testing.T) {
	if !menuKeyPressed(rl.KeyD, rl.KeyD) {
		t.Fatal("captured D key should activate the day/night shortcut")
	}
}

func TestShortcutDockShowsCurrentSkyMode(t *testing.T) {
	oldOverride := islandDayNightOverride
	t.Cleanup(func() { islandDayNightOverride = oldOverride })

	tests := []struct {
		override int
		want     string
	}{
		{override: -1, want: "Sky Auto"},
		{override: 0, want: "Sky Day"},
		{override: 1, want: "Sky Night"},
	}
	for _, test := range tests {
		islandDayNightOverride = test.override
		got := ""
		for _, item := range shortcutDockItems(false) {
			if item.key == "D" {
				got = item.action
				break
			}
		}
		if got != test.want {
			t.Errorf("D shortcut for override %d = %q, want %q", test.override, got, test.want)
		}
	}
}

func TestMissingIntroScreenFailsExplicitly(t *testing.T) {
	savedCount := numScrResources
	numScrResources = 0
	defer func() { numScrResources = savedCount }()

	defer func() {
		recovered := recover()
		if got, want := fmt.Sprint(recovered), "scr resource: INTRO.SCR not found"; got != want {
			t.Fatalf("missing intro failure = %q, want %q", got, want)
		}
	}()
	findSCRResource("INTRO.SCR")
}

func TestWalkInitAtDestinationTurnsInPlace(t *testing.T) {
	walkInit(0, 2, 0, 6)
	if currentSpot != 0 || finalSpot != 0 || nextSpot != -1 || nextHdg != 6 || lastTurn != 1 || hasArrived != 0 {
		t.Fatalf("same-spot walk state = current %d final %d nextSpot %d nextHdg %d lastTurn %d arrived %d",
			currentSpot, finalSpot, nextSpot, nextHdg, lastTurn, hasArrived)
	}
}

func TestWalkingPathsAreBoundedAndOrdered(t *testing.T) {
	for from := 0; from < NumOfNodes; from++ {
		for to := 0; to < NumOfNodes; to++ {
			path := calcPath(from, to)
			if len(path) < 2 || path[0] != from {
				t.Fatalf("path %d -> %d starts with %#v", from, to, path)
			}
			terminator := -1
			for index, node := range path {
				if node == UndefNode {
					terminator = index
					break
				}
				if node < 0 || node >= NumOfNodes {
					t.Fatalf("path %d -> %d contains invalid node %d", from, to, node)
				}
			}
			if terminator < 1 || path[terminator-1] != to {
				t.Fatalf("path %d -> %d does not end at its destination: %#v", from, to, path)
			}
			if terminator+1 != len(path) {
				t.Fatalf("path %d -> %d contains frames after its terminator: %#v", from, to, path)
			}
		}
	}
}

func TestWalkingFrameSequencesStayInSourceOrder(t *testing.T) {
	for from := 0; from < NumOfNodes; from++ {
		for to := 0; to < NumOfNodes; to++ {
			start := walkDataBookmarks[from][to]
			if start < 0 {
				continue
			}
			ended := false
			for index := start; index < len(walkData); index++ {
				frame := walkData[index]
				if frame[1] == 0 {
					ended = true
					break
				}
				if frame[3] > 32 {
					t.Fatalf("walk %d -> %d frame %d uses sprite %d outside JOHNWALK order", from, to, index-start, frame[3])
				}
			}
			if !ended {
				t.Fatalf("walk %d -> %d has no terminal frame", from, to)
			}
		}
	}
}

func TestWalkingTreeOcclusionIsFixedAndOrdered(t *testing.T) {
	frame := [4]uint16{1, 400, 230, 23}
	items := walkDrawItems(frame, true)
	if len(items) != 3 {
		t.Fatalf("behind-tree draw count = %d, want character, trunk, leaves", len(items))
	}
	if items[0].tree || items[0].sprite != 23 || items[0].x != 399 || items[0].y != 230 || !items[0].flipped {
		t.Fatalf("character draw item = %#v", items[0])
	}
	if !items[1].tree || items[1].sprite != walkTreeTrunkSprite || items[1].x != 442 || items[1].y != 148 {
		t.Fatalf("tree trunk draw item = %#v", items[1])
	}
	if !items[2].tree || items[2].sprite != walkTreeLeavesSprite || items[2].x != 365 || items[2].y != 122 {
		t.Fatalf("tree leaves draw item = %#v", items[2])
	}

	oldIslandX, oldIslandY := islandState.xPos, islandState.yPos
	oldDX, oldDY := grDx, grDy
	t.Cleanup(func() {
		islandState.xPos, islandState.yPos = oldIslandX, oldIslandY
		grDx, grDy = oldDX, oldDY
	})
	islandState.xPos, islandState.yPos = 7, -3
	grDx, grDy = 400, 500
	resetWalkDrawOffset()
	if grDx != 7 || grDy != -3 {
		t.Fatalf("walk/tree offset = (%d,%d), want stable island offset (7,-3)", grDx, grDy)
	}
}

func TestTTMPlacementFollowsIsland(t *testing.T) {
	oldIsland, oldDX, oldDY := islandState, ttmDx, ttmDy
	t.Cleanup(func() {
		islandState, ttmDx, ttmDy = oldIsland, oldDX, oldDY
	})

	islandState.xPos = -137
	islandState.yPos = 42
	ttmDx, ttmDy = 999, 999 // Simulate placement inherited from prior content.

	ttmFollowIslandPlacement(0)
	if ttmDx != -137 || ttmDy != 42 {
		t.Fatalf("selected TTM placement = (%d,%d), want current island (-137,42)", ttmDx, ttmDy)
	}

	ttmFollowIslandPlacement(272)
	if ttmDx != 135 || ttmDy != 42 {
		t.Fatalf("left-island scene placement = (%d,%d), want (135,42)", ttmDx, ttmDy)
	}

	ttmUseStandalonePlacement()
	if islandState.xPos != 0 || islandState.yPos != 0 || ttmDx != 0 || ttmDy != 0 {
		t.Fatalf("selected standalone TTM kept placement: island=(%d,%d), TTM=(%d,%d)", islandState.xPos, islandState.yPos, ttmDx, ttmDy)
	}
}

func TestNextTTMSceneIndex(t *testing.T) {
	slot := &TTtmSlot{
		dataSize: 100,
		numTags:  5,
		tags: []TTtmTag{
			{id: 0, offset: 5},
			{id: 1, offset: 10},
			{id: 2, offset: 40},
			{id: 3, offset: 100},
			{id: 4, offset: 80},
		},
	}

	tests := []struct {
		name   string
		ip     uint32
		want   int
		wantOK bool
	}{
		{name: "before first scene", ip: 0, want: 1, wantOK: true},
		{name: "inside first scene", ip: 20, want: 2, wantOK: true},
		{name: "wrap after last scene", ip: 90, want: 1, wantOK: true},
	}

	for _, test := range tests {
		t.Run(test.name, func(t *testing.T) {
			got, ok := nextTTMSceneIndex(slot, test.ip)
			if got != test.want || ok != test.wantOK {
				t.Fatalf("nextTTMSceneIndex(%d) = (%d, %t), want (%d, %t)", test.ip, got, ok, test.want, test.wantOK)
			}
		})
	}

	if _, ok := nextTTMSceneIndex(&TTtmSlot{}, 0); ok {
		t.Fatal("nextTTMSceneIndex() reported a scene for an empty slot")
	}
}

func TestMenuRunNextTTMWraps(t *testing.T) {
	oldEntries, oldContent, oldSelection := menuEntries, currentContent, menuSelection
	t.Cleanup(func() {
		menuEntries, currentContent, menuSelection = oldEntries, oldContent, oldSelection
	})

	menuEntries = []menuEntry{
		{label: "Full story", target: ""},
		{label: "A.TTM", target: "A.TTM"},
		{label: "B.TTM", target: "B.TTM"},
	}
	currentContent = "B.TTM"

	defer func() {
		recovered := recover()
		change, ok := recovered.(contentSwitch)
		if !ok {
			t.Fatalf("menuRunNextTTM() panic = %#v, want contentSwitch", recovered)
		}
		if change.target != "A.TTM" {
			t.Fatalf("menuRunNextTTM() target = %q, want A.TTM", change.target)
		}
		if menuSelection != 1 {
			t.Fatalf("menuRunNextTTM() selection = %d, want 1", menuSelection)
		}
	}()

	menuRunNextTTM()
	t.Fatal("menuRunNextTTM() did not request a content switch")
}

func TestTTMCatalogKeepsFriendlyLabelsSeparateFromResourceNames(t *testing.T) {
	if len(ttmCatalog) != 41 {
		t.Fatalf("TTM catalog contains %d entries, want 41", len(ttmCatalog))
	}
	item := ttmCatalogInfo("MJFIRE.TTM")
	if item.label != "Building a Campfire" {
		t.Fatalf("MJFIRE.TTM label = %q", item.label)
	}
	if item.description == "" {
		t.Fatal("MJFIRE.TTM description is empty")
	}
	entry := menuEntry{label: item.label, description: item.description, target: "MJFIRE.TTM"}
	if got := menuEntryTitle(entry); got != "Building a Campfire (MJFIRE.TTM)" {
		t.Fatalf("menu title = %q", got)
	}
}

func TestTTMCatalogFallsBackToResourceName(t *testing.T) {
	item := ttmCatalogInfo("UNKNOWN.TTM")
	if item.label != "UNKNOWN.TTM" || item.description == "" {
		t.Fatalf("fallback catalog item = %#v", item)
	}
}

func TestTTMLoadScansStringBearingOpcode(t *testing.T) {
	data := make([]byte, 0, 20)
	data = binary.LittleEndian.AppendUint16(data, 0xF02F)
	data = append(data, []byte("FLAME.BMP")...)
	data = append(data, 0)
	data = binary.LittleEndian.AppendUint16(data, 0x1111)
	data = binary.LittleEndian.AppendUint16(data, 7)
	data = binary.LittleEndian.AppendUint16(data, 0x0FF0)

	oldResources, oldCount := ttmResources, numTtmResources
	t.Cleanup(func() { ttmResources, numTtmResources = oldResources, oldCount })
	ttmResources = make([]TTTMResource, MaxTTMResources)
	ttmResources[0] = TTTMResource{
		ResName:          "STRING.TTM",
		UncompressedSize: uint32(len(data)),
		UncompressedData: data,
		NumTags:          1,
	}
	numTtmResources = 1

	var slot TTtmSlot
	ttmLoadTTM(&slot, "STRING.TTM")
	if slot.numTags != 1 || slot.tags[0].id != 7 || slot.tags[0].offset != 16 {
		t.Fatalf("ttmLoadTTM() tags = %#v, want tag 7 at offset 16", slot.tags)
	}
}

func TestMissingFireSpritesAreSkipped(t *testing.T) {
	oldResources, oldCount := bmpResources, numBmpResources
	t.Cleanup(func() { bmpResources, numBmpResources = oldResources, oldCount })
	bmpResources = make([]TBMPResource, MaxBMPResources)
	numBmpResources = 0

	for _, name := range []string{"FLAME.BMP", "FLURRY.BMP"} {
		t.Run(name, func(t *testing.T) {
			slot := &TTtmSlot{}
			grLoadBmp(slot, 0, name)
			if slot.numSprites[0] != 0 {
				t.Fatalf("grLoadBmp(%q) loaded %d sprites, want 0", name, slot.numSprites[0])
			}
		})
	}
}

func TestFireTTMMissingSpriteUsage(t *testing.T) {
	dataDirectory := resetEmbeddedResourcesForTest(t)
	parseResourceFiles(filepath.Join(dataDirectory, "RESOURCE.MAP"))
	resource := findTTMResource("FIRE.TTM")
	data := resource.UncompressedData
	selectedSlot := uint16(0)
	loaded := make(map[uint16]string)
	missingDraws := make(map[string]int)
	maxFrame := make(map[string]uint16)

	for offset := 0; offset+2 <= len(data); {
		opcode := binary.LittleEndian.Uint16(data[offset:])
		offset += 2
		argCount := int(opcode & 0x000f)
		if argCount == 0x0f {
			start := offset
			for offset < len(data) && data[offset] != 0 {
				offset++
			}
			value := string(data[start:offset])
			offset++
			if offset%2 != 0 {
				offset++
			}
			if opcode == 0xF02F {
				loaded[selectedSlot] = value
			}
			continue
		}
		if offset+argCount*2 > len(data) {
			t.Fatalf("FIRE.TTM has truncated opcode %#04x", opcode)
		}
		args := make([]uint16, argCount)
		for i := range args {
			args[i] = binary.LittleEndian.Uint16(data[offset+i*2:])
		}
		if opcode == 0x1051 {
			selectedSlot = args[0]
		}
		if opcode == 0xA504 || opcode == 0xA524 {
			name := loaded[args[3]]
			if name == "FLAME.BMP" || name == "FLURRY.BMP" {
				missingDraws[name]++
				if args[2] > maxFrame[name] {
					maxFrame[name] = args[2]
				}
				t.Logf("%s frame=%d x=%d y=%d flipped=%t", name, args[2], int16(args[0]), int16(args[1]), opcode == 0xA524)
			}
		}
		offset += argCount * 2
	}

	for _, name := range []string{"FLAME.BMP", "FLURRY.BMP"} {
		if missingDraws[name] == 0 {
			t.Errorf("FIRE.TTM never draws %s", name)
		}
		t.Logf("%s draws=%d maxFrame=%d", name, missingDraws[name], maxFrame[name])
	}
}

func TestLilliputianTTMSpriteUsage(t *testing.T) {
	dataDirectory := resetEmbeddedResourcesForTest(t)
	parseResourceFiles(filepath.Join(dataDirectory, "RESOURCE.MAP"))

	for _, resourceName := range []string{"GJGULIVR.TTM", "GJLILIPU.TTM"} {
		t.Run(resourceName, func(t *testing.T) {
			resource := findTTMResource(resourceName)
			data := resource.UncompressedData
			selectedSlot := uint16(0)
			loaded := make(map[uint16]*TBMPResource)
			loads := make(map[string]bool)
			draws := 0
			flippedDraws := 0
			maxSprite := make(map[string]uint16)
			for _, preload := range standaloneTTMSpritePreloads[resourceName] {
				bmp, found := lookupBMPResource(preload.resource)
				if !found {
					t.Fatalf("standalone preload %s is missing", preload.resource)
				}
				loaded[preload.slot] = bmp
				loads[preload.resource] = true
			}

			for offset := 0; offset+2 <= len(data); {
				instructionOffset := offset
				opcode := binary.LittleEndian.Uint16(data[offset:])
				offset += 2
				argCount := int(opcode & 0x000f)
				if argCount == 0x0f {
					start := offset
					for offset < len(data) && data[offset] != 0 {
						offset++
					}
					if offset >= len(data) {
						t.Fatalf("unterminated string opcode %#04x at %#x", opcode, instructionOffset)
					}
					value := string(data[start:offset])
					offset++
					if offset%2 != 0 {
						offset++
					}
					if opcode == 0xF02F {
						bmp, found := lookupBMPResource(value)
						if !found {
							t.Errorf("LOAD_IMAGE %s into slot %d has no original BMP resource", value, selectedSlot)
							delete(loaded, selectedSlot)
						} else {
							loaded[selectedSlot] = bmp
							loads[value] = true
						}
					}
					continue
				}
				if offset+argCount*2 > len(data) {
					t.Fatalf("truncated opcode %#04x at %#x", opcode, instructionOffset)
				}
				args := make([]uint16, argCount)
				for index := range args {
					args[index] = binary.LittleEndian.Uint16(data[offset+index*2:])
				}
				offset += argCount * 2

				if opcode == 0x1051 {
					selectedSlot = args[0]
				}
				if opcode != 0xA504 && opcode != 0xA524 {
					continue
				}
				draws++
				if opcode == 0xA524 {
					flippedDraws++
				}
				spriteNo, imageSlot := args[2], args[3]
				bmp := loaded[imageSlot]
				if bmp == nil {
					t.Errorf("draw at %#x uses unloaded image slot %d sprite %d", instructionOffset, imageSlot, spriteNo)
					continue
				}
				if spriteNo >= bmp.NumImages {
					t.Errorf("draw at %#x uses %s sprite %d, but it has %d sprites", instructionOffset, bmp.ResName, spriteNo, bmp.NumImages)
				}
				if spriteNo > maxSprite[bmp.ResName] {
					maxSprite[bmp.ResName] = spriteNo
				}
			}

			if draws == 0 || len(loads) == 0 {
				t.Fatalf("sprite audit found loads=%d draws=%d", len(loads), draws)
			}
			t.Logf("tags=%d image resources=%d draws=%d flipped=%d", resource.NumTags, len(loads), draws, flippedDraws)
			for name, highest := range maxSprite {
				t.Logf("%s highest referenced sprite=%d", name, highest)
			}
		})
	}
}

func TestGulliverStandaloneScene54KeepsSleepingJohnny(t *testing.T) {
	companions := standaloneTTMCompanionScenes["GJGULIVR.TTM"][54]
	if !reflect.DeepEqual(companions, []uint16{58}) {
		t.Fatalf("scene 54 companions = %v, want sleeping-Johnny scene 58", companions)
	}
}

func TestStandaloneCompositeEventsKeepADSCompanions(t *testing.T) {
	tests := []struct {
		name       string
		tag        uint16
		companions []uint16
	}{
		{name: "GJGULIVR.TTM", tag: 15, companions: []uint16{14}},
		{name: "GJGULIVR.TTM", tag: 9, companions: []uint16{58, 12}},
		{name: "GJGULIVR.TTM", tag: 67, companions: []uint16{60}},
		{name: "GJGULL1.TTM", tag: 26, companions: []uint16{41}},
		{name: "GJGULL1.TTM", tag: 46, companions: []uint16{35}},
		{name: "GJLILIPU.TTM", tag: 20, companions: []uint16{24}},
		{name: "GJLILIPU.TTM", tag: 25, companions: []uint16{20}},
		{name: "GJLILIPU.TTM", tag: 26, companions: []uint16{20, 25}},
		{name: "GJVIS3.TTM", tag: 52, companions: []uint16{44}},
		{name: "GJVIS5.TTM", tag: 7, companions: []uint16{1, 9}},
		{name: "GJVIS5W.TTM", tag: 3, companions: []uint16{1}},
		{name: "GJVIS6.TTM", tag: 5, companions: []uint16{4, 9}},
		{name: "GJDIVE.TTM", tag: 13, companions: []uint16{12}},
		{name: "GJNAT3.TTM", tag: 16, companions: []uint16{18}},
		{name: "MJCOCO.TTM", tag: 18, companions: []uint16{17, 33}},
		{name: "MJCOCO1.TTM", tag: 18, companions: []uint16{17}},
		{name: "MJCOCO1.TTM", tag: 20, companions: []uint16{19}},
		{name: "MJFISH.TTM", tag: 44, companions: []uint16{1, 43}},
		{name: "MJFISHC.TTM", tag: 58, companions: []uint16{43, 62}},
		{name: "MJBATH.TTM", tag: 24, companions: []uint16{42, 20, 30, 19, 14, 23}},
		{name: "MJSAND.TTM", tag: 84, companions: []uint16{1, 49, 57, 58, 59, 60}},
		{name: "MJFIRE.TTM", tag: 49, companions: []uint16{44, 144, 78}},
		{name: "SASKDATE.TTM", tag: 145, companions: []uint16{121, 135, 128, 150}},
		{name: "SJMSSGE.TTM", tag: 21, companions: []uint16{29, 6, 32}},
		{name: "MJRAFT.TTM", tag: 3, companions: []uint16{9}},
		{name: "SJGLIMPS.TTM", tag: 106, companions: []uint16{105}},
		{name: "SBREAKUP.TTM", tag: 19, companions: []uint16{33}},
		{name: "SJLEAVES.TTM", tag: 3, companions: []uint16{2}},
		{name: "SJWORK.TTM", tag: 3, companions: []uint16{9}},
		{name: "THEEND.TTM", tag: 8, companions: []uint16{9, 4, 6}},
		{name: "WOULDBE.TTM", tag: 2, companions: []uint16{1, 4}},
	}
	for _, test := range tests {
		if got := standaloneTTMCompanionScenes[test.name][test.tag]; !reflect.DeepEqual(got, test.companions) {
			t.Errorf("%s tag %d companions = %v, want %v", test.name, test.tag, got, test.companions)
		}
	}
}

func TestStandaloneCompositeEventTagsExist(t *testing.T) {
	dataDirectory := resetEmbeddedResourcesForTest(t)
	parseResourceFiles(filepath.Join(dataDirectory, "RESOURCE.MAP"))
	for name, scenes := range standaloneTTMCompanionScenes {
		slot := &TTtmSlot{}
		ttmLoadTTM(slot, name)
		for primary, companions := range scenes {
			if ttmFindTag(slot, primary) == 0 {
				t.Errorf("%s primary tag %d is missing", name, primary)
			}
			for _, companion := range companions {
				if ttmFindTag(slot, companion) == 0 {
					t.Errorf("%s companion tag %d for primary %d is missing", name, companion, primary)
				}
			}
		}
	}
}

func TestStandaloneADSEventCatalog(t *testing.T) {
	dataDirectory := resetEmbeddedResourcesForTest(t)
	parseResourceFiles(filepath.Join(dataDirectory, "RESOURCE.MAP"))

	tests := []struct {
		name  string
		count int
		ads   string
	}{
		{name: "GJGULIVR.TTM", count: 3, ads: "BUILDING.ADS"},
		{name: "MJFISH.TTM", count: 4, ads: "FISHING.ADS"},
		{name: "THEEND.TTM", count: 1, ads: "JOHNNY.ADS"},
		{name: "MEANWHIL.TTM", count: 4},
		{name: "GJLILIPU.TTM", count: 0},
	}
	for _, test := range tests {
		events := standaloneADSEventsForTTM(test.name)
		if len(events) != test.count {
			t.Errorf("%s ADS events = %d, want %d", test.name, len(events), test.count)
		}
		lastStoryIndex := -1
		for _, event := range events {
			if test.ads != "" && event.adsName != test.ads {
				t.Errorf("%s event uses %s, want %s", test.name, event.adsName, test.ads)
			}
			if event.description == "" {
				t.Errorf("%s event %s:%d has no display description", test.name, event.adsName, event.tag)
			}
			storyIndex := -1
			for index, scene := range storyScenes {
				if scene.adsName == event.adsName && uint16(scene.adsTagNo) == event.tag {
					storyIndex = index
					break
				}
			}
			if storyIndex <= lastStoryIndex {
				t.Errorf("%s events are not in original story order at %s:%d", test.name, event.adsName, event.tag)
			}
			lastStoryIndex = storyIndex
		}
	}
}

func TestStandaloneNextEventSelectionWraps(t *testing.T) {
	dataDirectory := resetEmbeddedResourcesForTest(t)
	parseResourceFiles(filepath.Join(dataDirectory, "RESOURCE.MAP"))
	oldTarget, oldIndex := standaloneEventTarget, standaloneEventIndex
	t.Cleanup(func() { standaloneEventTarget, standaloneEventIndex = oldTarget, oldIndex })
	standaloneResetEventMode()

	first, number, total, ok := standaloneSelectNextEvent("GJGULIVR.TTM")
	if !ok || number != 1 || total != 3 {
		t.Fatalf("first Gulliver event = (%d/%d, %t), want 1/3", number, total, ok)
	}
	for range total {
		standaloneSelectNextEvent("GJGULIVR.TTM")
	}
	wrapped, number, _, ok := standaloneSelectedEvent("GJGULIVR.TTM")
	if !ok || number != 1 || wrapped.adsName != first.adsName || wrapped.tag != first.tag {
		t.Fatalf("wrapped event = %#v (%d, %t), want first %#v", wrapped, number, ok, first)
	}
}

func TestGulliverStandaloneSceneJumpsRestoreSpriteSlots(t *testing.T) {
	dataDirectory := resetEmbeddedResourcesForTest(t)
	parseResourceFiles(filepath.Join(dataDirectory, "RESOURCE.MAP"))
	slot := &TTtmSlot{}
	ttmLoadTTM(slot, "GJGULIVR.TTM")

	tests := []struct {
		tag  uint16
		want map[uint16]string
	}{
		{tag: 58, want: map[uint16]string{3: "SLEEP.BMP"}},
		{tag: 54, want: map[uint16]string{1: "LILIPUTS.BMP", 3: "SLEEP.BMP", 5: "GJBIPLAN.BMP"}},
	}
	for _, test := range tests {
		offset := ttmFindTag(slot, test.tag)
		resources := ttmStandaloneSceneSpriteResources(slot, slot.name, offset)
		for imageSlot, resource := range test.want {
			if resources[imageSlot] != resource {
				t.Errorf("tag %d slot %d = %q, want %q", test.tag, imageSlot, resources[imageSlot], resource)
			}
		}
	}
}

func TestDominantBottomColor(t *testing.T) {
	pixels := []byte{
		1, 1, 1, 255, 2, 2, 2, 255, 3, 3, 3, 255,
		9, 8, 7, 255, 4, 5, 6, 255, 9, 8, 7, 255,
	}
	if got, want := dominantBottomColor(pixels, 3, 2), (color.RGBA{R: 9, G: 8, B: 7, A: 255}); got != want {
		t.Fatalf("dominantBottomColor() = %#v, want %#v", got, want)
	}

	if got, want := dominantBottomColor(nil, 0, 0), (color.RGBA{A: 255}); got != want {
		t.Fatalf("dominantBottomColor(empty) = %#v, want %#v", got, want)
	}
}

func TestRenderTextureCopyRect(t *testing.T) {
	got := grRenderTextureRect(10, 20, 30, 40, 480)
	if got.X != 10 || got.Y != 420 || got.Width != 30 || got.Height != -40 {
		t.Fatalf("grRenderTextureRect() = %#v, want {10 420 30 -40}", got)
	}
}

func TestEmbeddedDrawScreenOpcodesUseSupportedBuffers(t *testing.T) {
	dataDirectory := resetEmbeddedResourcesForTest(t)
	parseResourceFiles(filepath.Join(dataDirectory, "RESOURCE.MAP"))

	found := 0
	for resourceIndex := 0; resourceIndex < numTtmResources; resourceIndex++ {
		resource := &ttmResources[resourceIndex]
		data := resource.UncompressedData
		for offset := 0; offset+2 <= len(data); {
			instructionOffset := offset
			opcode := binary.LittleEndian.Uint16(data[offset:])
			offset += 2
			argCount := int(opcode & 0x000f)
			if argCount == 0x0f {
				for offset < len(data) && data[offset] != 0 {
					offset++
				}
				offset++
				if offset%2 != 0 {
					offset++
				}
				continue
			}
			if offset+argCount*2 > len(data) {
				t.Fatalf("%s has truncated opcode %#04x at %#x", resource.ResName, opcode, instructionOffset)
			}
			if opcode == 0xB606 {
				found++
				args := make([]uint16, argCount)
				for i := range args {
					args[i] = binary.LittleEndian.Uint16(data[offset+i*2:])
				}
				if len(args) != 6 || args[4] > ttmBufferComposition || args[5] > ttmBufferComposition {
					t.Errorf("%s DRAW_SCREEN at %#x has unsupported args %v", resource.ResName, instructionOffset, args)
				}
				t.Logf("%s DRAW_SCREEN at %#x: %v", resource.ResName, instructionOffset, args)
			}
			offset += argCount * 2
		}
	}
	if found == 0 {
		t.Fatal("embedded TTMs contain no DRAW_SCREEN instructions")
	}
}

func TestRotateLogIfNeeded(t *testing.T) {
	dir := t.TempDir()
	logPath := filepath.Join(dir, appLogName)
	oldBackup := []byte("old backup")
	current := []byte("current log at the size limit")
	if err := os.WriteFile(logPath+".1", oldBackup, 0644); err != nil {
		t.Fatal(err)
	}
	if err := os.WriteFile(logPath, current, 0644); err != nil {
		t.Fatal(err)
	}

	if err := rotateLogIfNeeded(logPath, int64(len(current))); err != nil {
		t.Fatalf("rotateLogIfNeeded() error = %v", err)
	}
	if _, err := os.Stat(logPath); !os.IsNotExist(err) {
		t.Fatalf("active log still exists after rotation: %v", err)
	}
	got, err := os.ReadFile(logPath + ".1")
	if err != nil {
		t.Fatal(err)
	}
	if string(got) != string(current) {
		t.Fatalf("backup = %q, want %q", got, current)
	}
}

func TestAllShortScreensHaveValidPaddingColor(t *testing.T) {
	dataDirectory := resetEmbeddedResourcesForTest(t)
	parseResourceFiles(filepath.Join(dataDirectory, "RESOURCE.MAP"))
	grLoadPalette(&palResources[0])

	shortScreens := 0
	for index := 0; index < numScrResources; index++ {
		resource := &scrResources[index]
		if resource.Height >= screenHeight {
			continue
		}
		shortScreens++
		pixels, width, height := screenPixelData(resource)
		fill := dominantBottomColor(pixels, width, height)
		if fill.A != 255 {
			t.Errorf("%s padding color alpha = %d, want 255", resource.ResName, fill.A)
		}
		t.Logf("%s: %dx%d, padding RGBA(%d,%d,%d,%d)", resource.ResName, width, height, fill.R, fill.G, fill.B, fill.A)
	}
	if shortScreens == 0 {
		t.Fatal("embedded resources contain no short screens to validate")
	}
}

func testDataDirectory(t *testing.T) string {
	t.Helper()
	candidates := []string{"assets", defaultDataDirectory()}
	for _, directory := range candidates {
		if validateDataDirectory(directory) == nil {
			return directory
		}
	}
	t.Skip("verified original user-supplied test data is unavailable in assets or scrantic")
	return ""
}

func resetEmbeddedResourcesForTest(t *testing.T) string {
	t.Helper()
	dataDirectory := testDataDirectory(t)
	if err := loadResourceArchives(dataDirectory); err != nil {
		t.Fatalf("load verified test data from %s: %v", dataDirectory, err)
	}
	oldADSResources, oldADSCount := adsResources, numAdsResources
	oldBMPResources, oldBMPCount := bmpResources, numBmpResources
	oldPALResources, oldPALCount := palResources, numPalResources
	oldSCRResources, oldSCRCount := scrResources, numScrResources
	oldTTMResources, oldTTMCount := ttmResources, numTtmResources
	t.Cleanup(func() {
		adsResources, numAdsResources = oldADSResources, oldADSCount
		bmpResources, numBmpResources = oldBMPResources, oldBMPCount
		scrResources, numScrResources = oldSCRResources, oldSCRCount
		palResources, numPalResources = oldPALResources, oldPALCount
		ttmResources, numTtmResources = oldTTMResources, oldTTMCount
	})

	adsResources = make([]TAdsResource, MaxADSResources)
	bmpResources = make([]TBMPResource, MaxBMPResources)
	scrResources = make([]TSCRResource, MaxSCRResources)
	palResources = make([]TPALResource, MaxPALResources)
	ttmResources = make([]TTTMResource, MaxTTMResources)
	numAdsResources, numBmpResources, numPalResources, numScrResources, numTtmResources = 0, 0, 0, 0, 0
	return dataDirectory
}

func TestEmbeddedArchiveHashesAndDecompression(t *testing.T) {
	dataDirectory := resetEmbeddedResourcesForTest(t)
	tests := []struct {
		name string
		data []byte
		md5  string
	}{
		{name: "RESOURCE.MAP", data: resourceMapData, md5: canonicalMapMD5},
		{name: "RESOURCE.001", data: resourceArchiveData, md5: canonicalArchiveMD5},
	}
	for _, test := range tests {
		t.Run(test.name, func(t *testing.T) {
			got := fmt.Sprintf("%x", md5.Sum(test.data))
			if got != test.md5 {
				t.Fatalf("MD5 = %s, want verified archive %s", got, test.md5)
			}
		})
	}

	parseResourceFiles(filepath.Join(dataDirectory, "RESOURCE.MAP"))
	if numAdsResources != 10 || numBmpResources != 117 || numPalResources != 1 || numScrResources != 10 || numTtmResources != 41 {
		t.Fatalf("parsed resource counts ADS=%d BMP=%d PAL=%d SCR=%d TTM=%d", numAdsResources, numBmpResources, numPalResources, numScrResources, numTtmResources)
	}
	for index := 0; index < numAdsResources; index++ {
		resource := &adsResources[index]
		if len(resource.UncompressedData) != int(resource.UncompressedSize) {
			t.Errorf("%s decompressed to %d bytes, want %d", resource.ResName, len(resource.UncompressedData), resource.UncompressedSize)
		}
	}
	for index := 0; index < numBmpResources; index++ {
		resource := &bmpResources[index]
		if len(resource.UncompressedData) != int(resource.UncompressedSize) {
			t.Errorf("%s decompressed to %d bytes, want %d", resource.ResName, len(resource.UncompressedData), resource.UncompressedSize)
		}
	}
	for index := 0; index < numScrResources; index++ {
		resource := &scrResources[index]
		if len(resource.UncompressedData) != int(resource.UncompressedSize) {
			t.Errorf("%s decompressed to %d bytes, want %d", resource.ResName, len(resource.UncompressedData), resource.UncompressedSize)
		}
	}
	for index := 0; index < numTtmResources; index++ {
		resource := &ttmResources[index]
		if len(resource.UncompressedData) != int(resource.UncompressedSize) {
			t.Errorf("%s decompressed to %d bytes, want %d", resource.ResName, len(resource.UncompressedData), resource.UncompressedSize)
		}
	}
}

func TestValidateDataDirectory(t *testing.T) {
	dataDirectory := testDataDirectory(t)
	if err := validateDataDirectory(dataDirectory); err != nil {
		t.Fatalf("selected test data directory is invalid: %v", err)
	}

	empty := t.TempDir()
	if err := validateDataDirectory(empty); err == nil || !strings.Contains(err.Error(), "RESOURCE.MAP") {
		t.Fatalf("empty data directory error = %v, want RESOURCE.MAP failure", err)
	}

	corrupt := t.TempDir()
	if err := os.WriteFile(filepath.Join(corrupt, "RESOURCE.MAP"), []byte("not a resource map"), 0600); err != nil {
		t.Fatal(err)
	}
	if err := os.WriteFile(filepath.Join(corrupt, "RESOURCE.001"), []byte("not a resource archive"), 0600); err != nil {
		t.Fatal(err)
	}
	if err := validateDataDirectory(corrupt); err == nil || !strings.Contains(err.Error(), "MD5") {
		t.Fatalf("corrupt data directory error = %v, want MD5 failure", err)
	}
}

func TestResourceArchiveProfilesRejectMixedPairs(t *testing.T) {
	for _, profile := range resourceArchiveProfiles {
		matched, ok := resourceArchiveProfileForHashes(profile.mapMD5, profile.archiveMD5)
		if !ok || matched.name != profile.name {
			t.Fatalf("profile %q did not match its verified hashes", profile.name)
		}
	}
	if _, ok := resourceArchiveProfileForHashes(canonicalMapMD5, archiveOrgDataMD5); ok {
		t.Fatal("canonical map and Internet Archive data must not be accepted as a mixed pair")
	}
}

func TestCompactMiddle(t *testing.T) {
	if got := compactMiddle("E:\\ai\\Johnny\\scrantic", 80); got != "E:\\ai\\Johnny\\scrantic" {
		t.Fatalf("short path changed to %q", got)
	}
	got := compactMiddle("E:\\a-very-long-folder-name\\another-long-folder\\scrantic", 24)
	if len([]rune(got)) != 24 || !strings.Contains(got, "...") || !strings.HasPrefix(got, "E:\\") || !strings.HasSuffix(got, "scrantic") {
		t.Fatalf("compacted path = %q", got)
	}
}

func TestChooseDefaultDataDirectory(t *testing.T) {
	root := t.TempDir()
	executablePath := filepath.Join(root, "JohnnyProject", "build", "JohnnyCastaway.exe")
	workingDirectory := filepath.Join(root, "JohnnyProject")
	besideExecutable := filepath.Join(root, "JohnnyProject", "build", "scrantic")
	workspaceSibling := filepath.Join(root, "scrantic")

	validDirectory := workspaceSibling
	validator := func(directory string) bool {
		return filepath.Clean(directory) == filepath.Clean(validDirectory)
	}
	if got := chooseDefaultDataDirectory(executablePath, workingDirectory, validator); got != filepath.Clean(workspaceSibling) {
		t.Fatalf("development scrantic directory = %q, want %q", got, filepath.Clean(workspaceSibling))
	}

	validDirectory = besideExecutable
	if got := chooseDefaultDataDirectory(executablePath, workingDirectory, validator); got != filepath.Clean(besideExecutable) {
		t.Fatalf("portable scrantic directory = %q, want %q", got, filepath.Clean(besideExecutable))
	}

	if got := chooseDefaultDataDirectory(executablePath, workingDirectory, func(string) bool { return false }); got != filepath.Clean(besideExecutable) {
		t.Fatalf("missing-data fallback = %q, want %q", got, filepath.Clean(besideExecutable))
	}
}

func TestMalformedRLEFailsClearly(t *testing.T) {
	tests := []struct {
		name    string
		input   []byte
		inSize  uint32
		outSize uint32
	}{
		{name: "truncated literal", input: []byte{2, 0xAA}, inSize: 2, outSize: 2},
		{name: "run exceeds output", input: []byte{0x82, 0xAA}, inSize: 2, outSize: 1},
		{name: "declared input exceeds data", input: []byte{0x81, 0xAA}, inSize: 3, outSize: 1},
	}
	for _, test := range tests {
		t.Run(test.name, func(t *testing.T) {
			defer func() {
				recovered := recover()
				message := fmt.Sprint(recovered)
				if recovered == nil || !bytes.Contains([]byte(message), []byte("invalid RLE compressed resource")) {
					t.Fatalf("panic = %q, want clear RLE validation error", message)
				}
			}()
			uncompressRLE(bytes.NewReader(test.input), test.inSize, test.outSize)
		})
	}
}

func TestMalformedLZWFailsClearly(t *testing.T) {
	tests := []struct {
		name    string
		input   []byte
		inSize  uint32
		outSize uint32
	}{
		{name: "declared input exceeds data", input: []byte{0}, inSize: 2, outSize: 1},
		{name: "oversized output", input: []byte{0}, inSize: 1, outSize: maxUncompressedResourceSize + 1},
	}
	for _, test := range tests {
		t.Run(test.name, func(t *testing.T) {
			defer func() {
				recovered := recover()
				message := fmt.Sprint(recovered)
				if recovered == nil || !bytes.Contains([]byte(message), []byte("invalid LZW compressed resource")) {
					t.Fatalf("panic = %q, want clear LZW validation error", message)
				}
			}()
			uncompressLZW(bytes.NewReader(test.input), test.inSize, test.outSize)
		})
	}
}

func TestPersistentSettingsConfigRoundTrip(t *testing.T) {
	want := TConfig{
		CurrentDay:       7,
		CurrentDate:      196,
		CRTFilterEnabled: true,
		CRTMode:          "lottes",
		SmoothingEnabled: true,
		ScalingMode:      "scale2x",
		ScenesInOrder:    true,
		Windowed:         true,
		Mute:             true,
		Stretch:          true,
		Monitor:          2,
		FastCRTSharpness: 3,
		ShowPerformance:  true,
		DataDirectory:    "E:\\Johnny\\scrantic",
	}
	var got TConfig
	for _, line := range strings.Split(strings.TrimSpace(cfgFormat(&want)), "\n") {
		if err := cfgApplyLine(&got, line); err != nil {
			t.Fatalf("cfgApplyLine(%q): %v", line, err)
		}
	}
	if !reflect.DeepEqual(got, want) {
		t.Fatalf("config round trip = %#v, want %#v", got, want)
	}
	formatted := cfgFormat(&want)
	if !strings.Contains(formatted, "[Settings]") || !strings.HasPrefix(formatted, "; Johnny Castaway") {
		t.Fatalf("settings are not formatted as a documented INI file:\n%s", formatted)
	}
}

func TestApplicationAndScreensaverShareInstalledConfigPath(t *testing.T) {
	directory := filepath.Join("E:\\Games", "Johnny Castaway")
	executable := filepath.Join(directory, "JohnnyCastaway.exe")
	screensaver := filepath.Join(directory, "JohnnyCastaway.scr")
	want := filepath.Join("E:\\Games", "Johnny Castaway", "JohnnyCastaway.ini")
	if got := configPathBesideExecutable(executable); got != want {
		t.Fatalf("application config path = %q, want %q", got, want)
	}
	if got := configPathBesideExecutable(screensaver); got != want {
		t.Fatalf("screensaver config path = %q, want %q", got, want)
	}
}

func TestInstallerOwnsSingleSharedSettingsFile(t *testing.T) {
	defaultSettings, err := os.ReadFile(filepath.Join("installer", CfgFileName))
	if err != nil {
		t.Fatal(err)
	}
	var cfg TConfig
	for _, line := range strings.Split(strings.TrimSpace(string(defaultSettings)), "\n") {
		if err := cfgApplyLine(&cfg, line); err != nil {
			t.Fatalf("default installer settings line %q: %v", line, err)
		}
	}
	if cfg.DataDirectory != "" {
		t.Fatalf("installer data directory = %q, want automatic managed-directory selection", cfg.DataDirectory)
	}
	if cfg.Monitor != 1 || cfg.FastCRTSharpness != 2 {
		t.Fatalf("installer defaults = %#v, want monitor 1 and balanced Fast CRT sharpness", cfg)
	}

	installer, err := os.ReadFile(filepath.Join("installer", "JohnnyCastaway.iss"))
	if err != nil {
		t.Fatal(err)
	}
	text := string(installer)
	required := []string{
		`Source: "JohnnyCastaway.ini"; DestDir: "{app}"; Flags: onlyifdoesntexist`,
		`Type: files; Name: "{localappdata}\JohnnyCastaway\JohnnyCastaway.ini"`,
		`Type: files; Name: "{localappdata}\JohnnyCastaway\config.txt"`,
		`Type: files; Name: "{%USERPROFILE}\.johnny_castaway_2026"`,
		`Type: files; Name: "{app}\JohnnyCastaway.ini"`,
		`The application and screensaver share one JohnnyCastaway.ini file.`,
	}
	for _, entry := range required {
		if !strings.Contains(text, entry) {
			t.Errorf("installer is missing %q", entry)
		}
	}
}

func TestWindowsArtifactsUseCastawayLookoutIcon(t *testing.T) {
	resourceLine := `1 ICON "../../assets/icons/candidates/castaway-lookout.ico"`
	for _, path := range []string{
		filepath.Join("build", "windows", "JohnnyCastaway.rc"),
		filepath.Join("build", "windows", "JohnnyCastaway-screensaver.rc"),
	} {
		contents, err := os.ReadFile(path)
		if err != nil {
			t.Fatal(err)
		}
		if !strings.Contains(string(contents), resourceLine) {
			t.Errorf("%s does not embed Castaway Lookout", path)
		}
	}

	installer, err := os.ReadFile(filepath.Join("installer", "JohnnyCastaway.iss"))
	if err != nil {
		t.Fatal(err)
	}
	if !strings.Contains(string(installer), `SetupIconFile=..\assets\icons\candidates\castaway-lookout.ico`) {
		t.Error("installer does not use Castaway Lookout")
	}
}

func TestMergePersistentAppOptions(t *testing.T) {
	cfg := TConfig{Windowed: true, Mute: true, Stretch: true, Monitor: 3}
	got := mergePersistentAppOptions(appOptions{monitor: 1}, cfg, nil)
	want := appOptions{windowed: true, mute: true, stretch: true, monitor: 3}
	if !reflect.DeepEqual(got, want) {
		t.Fatalf("merged options = %#v, want %#v", got, want)
	}

	overridden := mergePersistentAppOptions(
		appOptions{monitor: 2}, cfg,
		[]string{"--fullscreen", "--sound", "--fit", "--monitor=2"},
	)
	if overridden.windowed || overridden.mute || overridden.stretch || overridden.monitor != 2 {
		t.Fatalf("explicit overrides were replaced: %#v", overridden)
	}

	screensaver := mergePersistentAppOptions(appOptions{screenSaver: true, monitor: 1}, cfg, nil)
	if screensaver.windowed {
		t.Fatal("screensaver inherited persisted windowed mode")
	}
	if !screensaver.mute || !screensaver.stretch || screensaver.monitor != 3 {
		t.Fatalf("screensaver did not inherit shared settings: %#v", screensaver)
	}
}

func TestScalingModeMigrationAndLabels(t *testing.T) {
	if got := parseImageScalingMode("", true); got != scalingBilinear {
		t.Fatalf("legacy smoothing migrated to %q, want bilinear", got)
	}
	if got := parseImageScalingMode("scale2x", false); got != scalingScale2x {
		t.Fatalf("scaling mode = %q, want scale2x", got)
	}
	if got := parseImageScalingMode("invalid", false); got != scalingNearest {
		t.Fatalf("invalid scaling mode = %q, want nearest", got)
	}
}

func TestCRTFilterMigrationAndLabels(t *testing.T) {
	if got := parseCRTFilter("", true); got != crtLightweight {
		t.Fatalf("legacy CRT setting migrated to %q, want lightweight", got)
	}
	if got := parseCRTFilter("lottes", false); got != crtLottes {
		t.Fatalf("CRT mode = %q, want lottes", got)
	}
	if got := parseCRTFilter("fast", false); got != crtFast {
		t.Fatalf("CRT mode = %q, want fast", got)
	}
	if got := parseCRTFilter("hdr", false); got != crtHDR {
		t.Fatalf("HDR mode = %q, want hdr", got)
	}
	if got := parseCRTFilter("invalid", false); got != crtOff {
		t.Fatalf("invalid CRT mode = %q, want off", got)
	}
	if crtLottes.label() != "Lottes" {
		t.Fatalf("Lottes label = %q", crtLottes.label())
	}
	if crtHDR.label() != "HDR Pop" {
		t.Fatalf("HDR label = %q", crtHDR.label())
	}
}

func TestFastCRTSharpnessPersistence(t *testing.T) {
	if got := decodeFastCRTSharpness(0); got != fastCRTBalanced {
		t.Fatalf("missing sharpness decoded as %d, want balanced", got)
	}
	for mode := fastCRTSoft; mode <= fastCRTSharp; mode++ {
		if got := decodeFastCRTSharpness(encodeFastCRTSharpness(mode)); got != mode {
			t.Errorf("sharpness %d round trip = %d", mode, got)
		}
	}
	if fastCRTSharpnessValue(fastCRTSoft) >= fastCRTSharpnessValue(fastCRTSharp) {
		t.Fatal("soft sharpness is not lower than sharp preset")
	}
}

func TestPerformanceImpactAndCapacity(t *testing.T) {
	if got := int(math.Round(performanceBudgetPercent(10))); got != 30 {
		t.Fatalf("10 ms budget = %d%%, want 30%%", got)
	}
	tests := []struct {
		ms   float64
		want string
	}{
		{ms: 2, want: "Very low"},
		{ms: 8, want: "Low"},
		{ms: 16, want: "Moderate"},
		{ms: 25, want: "High"},
		{ms: 31, want: "Critical"},
	}
	for _, test := range tests {
		if got := performanceImpactLabel(test.ms); got != test.want {
			t.Errorf("impact at %.1f ms = %q, want %q", test.ms, got, test.want)
		}
	}
	originalFilter := crtFilterMode
	t.Cleanup(func() { crtFilterMode = originalFilter })
	crtFilterMode = crtHDR
	if got := performanceExpectedImpact(); got != "Moderate" {
		t.Fatalf("HDR Pop expected GPU impact = %q, want Moderate", got)
	}
}

func TestPerformanceBenchmarkModeOrder(t *testing.T) {
	want := []crtFilter{crtOff, crtLightweight, crtFast, crtHDR, crtLottes}
	if !reflect.DeepEqual(performanceBenchmarkModes, want) {
		t.Fatalf("benchmark modes = %#v, want %#v", performanceBenchmarkModes, want)
	}
}

func TestScreensaverExitPolicy(t *testing.T) {
	if !shouldExitScreenSaver(true, false, false) {
		t.Fatal("mouse movement outside an overlay should exit")
	}
	if shouldExitScreenSaver(true, true, false) {
		t.Fatal("mouse movement over an interactive overlay should not exit")
	}
	if !shouldExitScreenSaver(false, true, true) {
		t.Fatal("unlisted keyboard input should exit even with an overlay open")
	}
	if shouldExitScreenSaver(false, false, false) {
		t.Fatal("screensaver exited without input")
	}
}

func TestDoubleEscapeQuitWindow(t *testing.T) {
	if doubleEscapeShouldExit(false, 0, 0.5) {
		t.Fatal("an unarmed Escape press requested exit")
	}
	if !doubleEscapeShouldExit(true, 10, 11.5) {
		t.Fatal("second Escape at the window boundary did not request exit")
	}
	if doubleEscapeShouldExit(true, 10, 11.5001) {
		t.Fatal("expired double-Escape sequence requested exit")
	}
	if doubleEscapeShouldExit(true, 10, 9) {
		t.Fatal("time moving backwards requested exit")
	}
}

func TestScreensaverFooterOpacity(t *testing.T) {
	tests := []struct {
		elapsed float64
		want    float32
	}{
		{elapsed: 0, want: 1},
		{elapsed: 8, want: 1},
		{elapsed: 9, want: 0.5},
		{elapsed: 10, want: 0},
		{elapsed: 12, want: 0},
	}
	for _, test := range tests {
		if got := menuFooterOpacity(test.elapsed); got != test.want {
			t.Errorf("menuFooterOpacity(%v) = %v, want %v", test.elapsed, got, test.want)
		}
	}
}

func TestInformationalUIActivityAndWake(t *testing.T) {
	originalLastActivity := uiLastActivity
	originalPreviousMouse := uiPreviousMouse
	originalMouseInitialized := uiMouseInitialized
	originalFooterStarted := menuFooterStarted
	t.Cleanup(func() {
		uiLastActivity = originalLastActivity
		uiPreviousMouse = originalPreviousMouse
		uiMouseInitialized = originalMouseInitialized
		menuFooterStarted = originalFooterStarted
	})

	start := rl.NewVector2(100, 100)
	uiInitializeActivity(20, start)
	if got := informationalUIOpacity(28); got != 1 {
		t.Fatalf("opacity after eight idle seconds = %v, want 1", got)
	}
	if got := informationalUIOpacity(29); got != 0.5 {
		t.Fatalf("opacity during idle fade = %v, want 0.5", got)
	}
	if got := informationalUIOpacity(30); got != 0 {
		t.Fatalf("opacity after ten idle seconds = %v, want 0", got)
	}
	if uiObserveActivity(31, start, false, false) {
		t.Fatal("unchanged input was treated as activity")
	}
	if !uiObserveActivity(32, rl.NewVector2(101, 100), false, false) {
		t.Fatal("mouse movement did not wake informational UI")
	}
	if got := informationalUIOpacity(32); got != 1 {
		t.Fatalf("opacity after mouse wake = %v, want 1", got)
	}
	if !uiObserveActivity(43, rl.NewVector2(101, 100), true, false) {
		t.Fatal("mouse button did not wake informational UI")
	}
	if !uiObserveActivity(54, rl.NewVector2(101, 100), false, true) {
		t.Fatal("keyboard input did not wake informational UI")
	}
	if menuFooterStarted != 54 {
		t.Fatalf("footer wake time = %v, want 54", menuFooterStarted)
	}
}

func TestShortcutDockShowsPrimaryControls(t *testing.T) {
	keys := func(items []shortcutDockItem) map[string]bool {
		result := make(map[string]bool, len(items))
		for _, item := range items {
			result[item.key] = true
		}
		return result
	}

	desktop := keys(shortcutDockItems(false))
	for _, key := range []string{"F1", "F2", "F3", "F4", "F5", "F7", "F8", "F9", "F10", "F12", "Space", "F", "D", "N", "T", "H", "↑ ↓", "Enter", "Esc ×2"} {
		if !desktop[key] {
			t.Errorf("desktop shortcut dock is missing %q", key)
		}
	}
	screenSaver := keys(shortcutDockItems(true))
	if screenSaver["F"] {
		t.Fatal("screensaver shortcut dock advertises unavailable fullscreen control")
	}
	if len(screenSaver) != len(desktop)-1 {
		t.Fatalf("screensaver shortcut count = %d, desktop = %d", len(screenSaver), len(desktop))
	}
}

func TestPlaybackPauseStateAndLabel(t *testing.T) {
	originalPaused := playbackPaused
	originalAudioReady := audioReady
	originalStatus := menuStatusText
	t.Cleanup(func() {
		playbackPaused = originalPaused
		audioReady = originalAudioReady
		menuStatusText = originalStatus
	})
	audioReady = false
	setPlaybackPaused(true)
	if !playbackPaused || pauseShortcutLabel(playbackPaused) != "Resume" {
		t.Fatalf("paused state = %t, label = %q", playbackPaused, pauseShortcutLabel(playbackPaused))
	}
	setPlaybackPaused(false)
	if playbackPaused || pauseShortcutLabel(playbackPaused) != "Pause" {
		t.Fatalf("resumed state = %t, label = %q", playbackPaused, pauseShortcutLabel(playbackPaused))
	}
}

func TestScreenshotFilename(t *testing.T) {
	when := time.Date(2026, time.July, 17, 19, 45, 12, 345000000, time.UTC)
	if got, want := screenshotFilename(when), "Johnny-Castaway-20260717-194512.345.png"; got != want {
		t.Fatalf("screenshot filename = %q, want %q", got, want)
	}
}

func TestPNGMetadataRoundTrip(t *testing.T) {
	var source bytes.Buffer
	imageData := image.NewRGBA(image.Rect(0, 0, 1, 1))
	imageData.Set(0, 0, color.RGBA{R: 12, G: 34, B: 56, A: 255})
	if err := png.Encode(&source, imageData); err != nil {
		t.Fatal(err)
	}
	tags := []pngTextTag{
		{key: "Display Filter", value: "HDR Pop"},
		{key: "Image Scaling", value: "Scale2x"},
		{key: "Aspect Mode", value: "Fit 4:3"},
	}
	annotated, err := annotatePNG(source.Bytes(), tags)
	if err != nil {
		t.Fatal(err)
	}
	if _, err := png.Decode(bytes.NewReader(annotated)); err != nil {
		t.Fatalf("annotated PNG no longer decodes: %v", err)
	}
	metadata, err := pngTextMetadata(annotated)
	if err != nil {
		t.Fatal(err)
	}
	for _, tag := range tags {
		if got := metadata[tag.key]; got != tag.value {
			t.Errorf("metadata %q = %q, want %q", tag.key, got, tag.value)
		}
	}
}

func TestPNGMetadataRejectsInvalidInput(t *testing.T) {
	if _, err := annotatePNG([]byte("not a png"), []pngTextTag{{key: "Title", value: "Johnny"}}); err == nil {
		t.Fatal("invalid PNG was accepted")
	}
	if _, err := makePNGTextChunk(pngTextTag{key: "", value: "Johnny"}); err == nil {
		t.Fatal("empty PNG metadata key was accepted")
	}
}

func TestDisplayViewportAt32By9Resolutions(t *testing.T) {
	tests := []struct {
		width, height       float32
		wantX, wantW, wantH float32
	}{
		{width: 5120, height: 1440, wantX: 1600, wantW: 1920, wantH: 1440},
		{width: 7680, height: 2160, wantX: 2400, wantW: 2880, wantH: 2160},
	}
	for _, test := range tests {
		viewport := calculateDisplayViewport(test.width, test.height, false)
		if viewport.x != test.wantX || viewport.y != 0 || viewport.width != test.wantW || viewport.height != test.wantH {
			t.Errorf("viewport for %.0fx%.0f = %#v", test.width, test.height, viewport)
		}
	}
	stretched := calculateDisplayViewport(5120, 1440, true)
	if stretched != (displayViewport{width: 5120, height: 1440}) {
		t.Fatalf("stretched viewport = %#v", stretched)
	}
}

func TestMonitorSelectionAnd32By9Detection(t *testing.T) {
	if got := resolveMonitorIndex(2, 3); got != 1 {
		t.Fatalf("monitor index = %d, want 1", got)
	}
	if got := resolveMonitorIndex(4, 3); got != 0 {
		t.Fatalf("unavailable monitor index = %d, want fallback 0", got)
	}
	if !is32By9Display(5120, 1440) || !is32By9Display(7680, 2160) {
		t.Fatal("expected both Neo G9-class resolutions to be detected as 32:9")
	}
	if is32By9Display(3840, 2160) {
		t.Fatal("16:9 display detected as 32:9")
	}
}

func TestDefaultWindowedSize(t *testing.T) {
	tests := []struct {
		monitorWidth, monitorHeight int
		wantWidth, wantHeight       int
	}{
		{monitorWidth: 1920, monitorHeight: 1080, wantWidth: 960, wantHeight: 720},
		{monitorWidth: 800, monitorHeight: 600, wantWidth: 800, wantHeight: 600},
		{monitorWidth: 5120, monitorHeight: 1440, wantWidth: 960, wantHeight: 720},
	}
	for _, test := range tests {
		width, height := defaultWindowedSize(test.monitorWidth, test.monitorHeight)
		if width != test.wantWidth || height != test.wantHeight {
			t.Errorf("defaultWindowedSize(%d, %d) = (%d, %d), want (%d, %d)", test.monitorWidth, test.monitorHeight, width, height, test.wantWidth, test.wantHeight)
		}
	}
}

func TestDecodeADSScript(t *testing.T) {
	data := make([]byte, 0, 10)
	data = binary.LittleEndian.AppendUint16(data, 7)
	data = binary.LittleEndian.AppendUint16(data, 0x1360)
	data = binary.LittleEndian.AppendUint16(data, 1)
	data = binary.LittleEndian.AppendUint16(data, 2)
	data = binary.LittleEndian.AppendUint16(data, 0xFFFF)

	lines := decodeADSScript(data)
	if len(lines) != 3 {
		t.Fatalf("decodeADSScript() returned %d lines, want 3", len(lines))
	}
	if lines[0].offset != 0 || lines[0].instruction != "TAG 7" {
		t.Fatalf("tag line = %#v", lines[0])
	}
	if lines[1].offset != 2 || lines[1].instruction != "IF_NOT_RUNNING 1 2" {
		t.Fatalf("condition line = %#v", lines[1])
	}
	if lines[2].offset != 8 || lines[2].instruction != "END" {
		t.Fatalf("end line = %#v", lines[2])
	}
}

func TestDecodeEveryEmbeddedADSScript(t *testing.T) {
	dataDirectory := resetEmbeddedResourcesForTest(t)
	parseResourceFiles(filepath.Join(dataDirectory, "RESOURCE.MAP"))
	for index := 0; index < numAdsResources; index++ {
		resource := &adsResources[index]
		lines := decodeADSScript(resource.UncompressedData)
		if len(lines) == 0 {
			t.Errorf("%s decoded to no ADS instructions", resource.ResName)
			continue
		}
		previous := uint32(0)
		for lineIndex, line := range lines {
			if lineIndex > 0 && line.offset <= previous {
				t.Errorf("%s offsets are not increasing at line %d", resource.ResName, lineIndex)
			}
			if strings.Contains(line.instruction, "<truncated>") {
				t.Errorf("%s has truncated instruction at offset %#x", resource.ResName, line.offset)
			}
			previous = line.offset
		}
	}
}

func TestEmbeddedADSConditionInventory(t *testing.T) {
	dataDirectory := resetEmbeddedResourcesForTest(t)
	parseResourceFiles(filepath.Join(dataDirectory, "RESOURCE.MAP"))
	conditionCount := 0
	for index := 0; index < numAdsResources; index++ {
		resource := &adsResources[index]
		lines := decodeADSScript(resource.UncompressedData)
		for lineIndex, line := range lines {
			if line.opcode < 0x1300 || line.opcode > 0x1430 {
				continue
			}
			conditionCount++
			t.Logf("%s: %s", resource.ResName, line.instruction)
			if line.opcode == 0x1420 || line.opcode == 0x1430 {
				if lineIndex == 0 || lineIndex+1 >= len(lines) {
					t.Errorf("%s has dangling %s", resource.ResName, line.instruction)
					continue
				}
				next := lines[lineIndex+1].opcode
				if next < 0x1300 || next >= 0x1400 {
					t.Errorf("%s %s is followed by %s", resource.ResName, line.instruction, lines[lineIndex+1].instruction)
				}
			}
		}
	}
	if conditionCount == 0 {
		t.Fatal("embedded ADS files contain no conditions")
	}
}

func TestEmbeddedADSLocalControlInventory(t *testing.T) {
	dataDirectory := resetEmbeddedResourcesForTest(t)
	parseResourceFiles(filepath.Join(dataDirectory, "RESOURCE.MAP"))
	found := 0
	for index := 0; index < numAdsResources; index++ {
		resource := &adsResources[index]
		lines := decodeADSScript(resource.UncompressedData)
		for lineIndex, line := range lines {
			if line.opcode != 0x1070 {
				continue
			}
			found++
			if lineIndex+2 >= len(lines) {
				t.Fatalf("%s has truncated WHILE_RUNNING block", resource.ResName)
			}
			if line.instruction != "WHILE_RUNNING 4 5" ||
				lines[lineIndex+1].instruction != "END_WHILE" ||
				lines[lineIndex+2].instruction != "ADD_SCENE 4 22 0 1" {
				t.Errorf("%s local wait decoded as %q / %q / %q", resource.ResName, line.instruction, lines[lineIndex+1].instruction, lines[lineIndex+2].instruction)
			}
		}
	}
	if found != 1 {
		t.Fatalf("found %d WHILE_RUNNING blocks, want 1", found)
	}
}

func TestADSConditionState(t *testing.T) {
	t.Run("AND remains false", func(t *testing.T) {
		var state adsConditionState
		state.add(true)
		state.setOperator(0x1420)
		state.add(false)
		state.setOperator(0x1420)
		state.add(true)
		if state.shouldExecute() {
			t.Fatal("true AND false AND true evaluated true")
		}
	})

	t.Run("OR accepts either branch", func(t *testing.T) {
		var state adsConditionState
		state.add(false)
		state.setOperator(0x1430)
		state.add(true)
		if !state.shouldExecute() {
			t.Fatal("false OR true evaluated false")
		}
	})

	t.Run("reset removes skipped block", func(t *testing.T) {
		var state adsConditionState
		state.add(false)
		state.reset()
		if !state.shouldExecute() {
			t.Fatal("reset condition still skips operations")
		}
	})
}

func TestADSConditionBlocksControlOperations(t *testing.T) {
	oldThreads, oldNumThreads := ttmThreads, numThreads
	oldStopRequested, oldPlayed := adsStopRequested, adsPlayedScenes
	t.Cleanup(func() {
		ttmThreads, numThreads = oldThreads, oldNumThreads
		adsStopRequested, adsPlayedScenes = oldStopRequested, oldPlayed
	})

	appendInstruction := func(data []byte, opcode uint16, args ...uint16) []byte {
		data = binary.LittleEndian.AppendUint16(data, opcode)
		for _, arg := range args {
			data = binary.LittleEndian.AppendUint16(data, arg)
		}
		return data
	}

	t.Run("failed AND skips stop", func(t *testing.T) {
		ttmThreads = [MaxTTMThreads]TTtmThread{}
		ttmThreads[0] = TTtmThread{isRunning: 1, sceneSlot: 1, sceneTag: 1}
		numThreads = 1
		data := appendInstruction(nil, 0x1360, 1, 1) // false
		data = appendInstruction(data, 0x1420)
		data = appendInstruction(data, 0x1360, 2, 2) // true
		data = appendInstruction(data, 0x2010, 1, 1, 0)
		data = appendInstruction(data, 0x1510)
		data = appendInstruction(data, 0xFFFF)
		adsPlayChunk(data, uint32(len(data)), 0)
		if ttmThreads[0].isRunning == 0 {
			t.Fatal("STOP_SCENE ran for a false AND condition")
		}
	})

	t.Run("successful OR runs stop", func(t *testing.T) {
		ttmThreads = [MaxTTMThreads]TTtmThread{}
		ttmThreads[0] = TTtmThread{isRunning: 1, sceneSlot: 1, sceneTag: 1}
		numThreads = 1
		data := appendInstruction(nil, 0x1370, 1, 1) // true
		data = appendInstruction(data, 0x1430)
		data = appendInstruction(data, 0x1370, 2, 2) // false
		data = appendInstruction(data, 0x2010, 1, 1, 0)
		data = appendInstruction(data, 0x1510)
		adsPlayChunk(data, uint32(len(data)), 0)
		if ttmThreads[0].isRunning != 0 {
			t.Fatal("STOP_SCENE was skipped for a true OR condition")
		}
	})
}

func TestTraceFilteringSearchAndADSHighlight(t *testing.T) {
	oldHistory, oldFilter, oldSearch := traceHistory, traceCurrentFilter, traceSearch
	oldView, oldADSName, oldADSLines := traceCurrentView, traceADSName, traceADSLines
	oldADSOffset, oldADSActive := traceADSActiveOffset, traceADSHasActive
	t.Cleanup(func() {
		traceHistory, traceCurrentFilter, traceSearch = oldHistory, oldFilter, oldSearch
		traceCurrentView, traceADSName, traceADSLines = oldView, oldADSName, oldADSLines
		traceADSActiveOffset, traceADSHasActive = oldADSOffset, oldADSActive
	})

	traceHistory = []ttmTraceEntry{
		{sequence: 1, source: traceSourceTTM, script: "FIRE.TTM", instruction: "LOAD_IMAGE FLAME.BMP"},
		{sequence: 2, source: traceSourceADS, script: "BUILDING.ADS", instruction: "IF_NOT_RUNNING 0 1"},
	}
	traceCurrentFilter = traceFilterADS
	if got := traceHistoryEntries(); len(got) != 1 || got[0].source != traceSourceADS {
		t.Fatalf("ADS filter returned %#v", got)
	}
	traceCurrentFilter = traceFilterAll
	traceSearch = "flame"
	if got := traceHistoryEntries(); len(got) != 1 || got[0].source != traceSourceTTM {
		t.Fatalf("search returned %#v", got)
	}

	traceCurrentView = traceViewADSScript
	traceSearch = ""
	traceADSName = "BUILDING.ADS"
	traceADSLines = []adsTraceLine{{offset: 4, opcode: 0x1360, instruction: "IF_NOT_RUNNING 0 1"}}
	traceADSActiveOffset = 4
	traceADSHasActive = true
	text := traceCurrentViewText()
	if !strings.Contains(text, "> 0004  1360  IF_NOT_RUNNING 0 1") {
		t.Fatalf("ADS export lacks active marker: %q", text)
	}
}

func TestAppVersionLabel(t *testing.T) {
	if label := appVersionLabel(); !strings.Contains(label, appVersion) || !strings.Contains(label, appBuildIdentifier()) {
		t.Fatalf("appVersionLabel() = %q", label)
	}
}
