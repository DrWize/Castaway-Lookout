package main

import (
	"bytes"
	"crypto/sha256"
	"encoding/binary"
	"fmt"
	"os"
	"path/filepath"
	"testing"
)

func canonicalFixtureResource(t *testing.T, name string) []byte {
	t.Helper()
	dataDir := filepath.Join("esp32", "data")
	if _, err := os.Stat(filepath.Join(dataDir, "RESOURCE.MAP")); os.IsNotExist(err) {
		t.Skip("licensed canonical Sierra inputs are not present")
	}
	mapData, archiveData, err := readVerifiedResourceArchives(dataDir)
	if err != nil {
		t.Fatal(err)
	}
	count := int(binary.LittleEndian.Uint16(mapData[19:21]))
	for index := 0; index < count; index++ {
		offset := int(binary.LittleEndian.Uint32(mapData[21+index*8+4:]))
		rawName := archiveData[offset : offset+13]
		if end := bytes.IndexByte(rawName, 0); end >= 0 {
			rawName = rawName[:end]
		}
		rawName = bytes.TrimRight(rawName, " ")
		size := int(binary.LittleEndian.Uint32(archiveData[offset+13 : offset+17]))
		if string(rawName) == name {
			return archiveData[offset+17 : offset+17+size]
		}
	}
	t.Fatalf("fixture resource %s was not found", name)
	return nil
}

func TestESPIntroFixtureMatchesDesktopDecoder(t *testing.T) {
	palette := parsePalResource(bytes.NewReader(canonicalFixtureResource(t, "JOHNCAST.PAL")))
	screen := parseScrResource(bytes.NewReader(canonicalFixtureResource(t, "INTRO.SCR")))
	if screen.Width != 640 || screen.Height != 480 {
		t.Fatalf("INTRO.SCR dimensions = %dx%d", screen.Width, screen.Height)
	}
	if got := fmt.Sprintf("%x", sha256.Sum256(screen.UncompressedData)); got != "6eb9bed1fa948b537652cc4f37da9f4733e828d174a52900dfea98932eafaea1" {
		t.Fatalf("desktop decoded fixture SHA-256 = %s", got)
	}

	previousPalette := ttmPalette
	defer func() { ttmPalette = previousPalette }()
	grLoadPalette(&palette)
	rgba, width, height := screenPixelData(&screen)
	rgb565 := make([]byte, 800*480*2)
	for y := 0; y < height; y++ {
		for x := 0; x < width; x++ {
			source := (y*width + x) * 4
			value := uint16(rgba[source]&0xf8)<<8 |
				uint16(rgba[source+1]&0xfc)<<3 | uint16(rgba[source+2])>>3
			destination := (y*800 + x) * 2
			binary.LittleEndian.PutUint16(rgb565[destination:], value)
		}
	}
	if got := fmt.Sprintf("%x", sha256.Sum256(rgb565)); got != "be6b74850333a3b2e9f1ecfadbfea7ada47d5b17b0e92b4ba09ff2f02e25ee40" {
		t.Fatalf("desktop right-layout RGB565 SHA-256 = %s", got)
	}
}
