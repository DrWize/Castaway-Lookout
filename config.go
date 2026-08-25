package main

import (
	"bufio"
	"fmt"
	"os"
	"path/filepath"
	"strconv"
	"strings"
	"sync"
)

// r.c. This is not idiomatic Go, but a mostly direct C port of the original source.
// A second pass refactor can be done to fix this garbage.
var (
	// r.c. - added by me in case someone tries to run multiple instances of the screensaver.
	cfgLock       sync.Mutex
	cfgActivePath string
)

type TConfig struct {
	CurrentDay       int
	CurrentDate      int
	CRTFilterEnabled bool
	CRTMode          string
	SmoothingEnabled bool
	ScalingMode      string
	ScenesInOrder    bool
	Windowed         bool
	Mute             bool
	Stretch          bool
	Monitor          int
	FastCRTSharpness int
	ShowPerformance  bool
	DataDirectory    string
}

const (
	CfgFileName         = "JohnnyCastaway.ini"
	CurrentDayKey       = "currentDay="
	DateKey             = "date="
	CRTFilterKey        = "crtFilter="
	CRTModeKey          = "crtMode="
	SmoothingKey        = "smoothing="
	ScalingModeKey      = "scalingMode="
	SceneOrderKey       = "scenesInOrder="
	WindowedKey         = "windowed="
	MuteKey             = "mute="
	StretchKey          = "stretch="
	MonitorKey          = "monitor="
	FastCRTSharpnessKey = "fastCRTSharpness="
	ShowPerformanceKey  = "showPerformance="
	DataDirectoryKey    = "dataDirectory="
)

func cfgFullPath() string {
	executable, err := os.Executable()
	if err != nil {
		panic(fmt.Errorf("executable path: %w", err))
	}
	return configPathBesideExecutable(executable)
}

func configPathBesideExecutable(executable string) string {
	return filepath.Join(filepath.Dir(executable), CfgFileName)
}

func cfgFileWrite(cfg *TConfig) {
	cfgLock.Lock()
	defer cfgLock.Unlock()
	cfgFileWriteUnlocked(cfg)
}

func cfgFileWriteUnlocked(cfg *TConfig) {
	data := cfgFormat(cfg)
	path := cfgFullPath()
	if err := os.WriteFile(path, []byte(data), 0644); err != nil {
		panic(fmt.Errorf("write shared settings %s: %w", path, err))
	}
	cfgActivePath = path
}

func cfgDisplayPath() string {
	cfgLock.Lock()
	defer cfgLock.Unlock()
	if cfgActivePath != "" {
		return cfgActivePath
	}
	return cfgFullPath()
}

func cfgFormat(cfg *TConfig) string {
	return fmt.Sprintf("; Johnny Castaway persistent settings\n; This file is safe to edit while Johnny Castaway is closed.\n\n[Settings]\n%s%d\n%s%d\n%s%d\n%s%s\n%s%d\n%s%s\n%s%d\n%s%d\n%s%d\n%s%d\n%s%d\n%s%d\n%s%d\n%s%s\n",
		CurrentDayKey, cfg.CurrentDay,
		DateKey, cfg.CurrentDate,
		CRTFilterKey, boolInt(cfg.CRTFilterEnabled),
		CRTModeKey, cfg.CRTMode,
		SmoothingKey, boolInt(cfg.SmoothingEnabled),
		ScalingModeKey, cfg.ScalingMode,
		SceneOrderKey, boolInt(cfg.ScenesInOrder),
		WindowedKey, boolInt(cfg.Windowed),
		MuteKey, boolInt(cfg.Mute),
		StretchKey, boolInt(cfg.Stretch),
		MonitorKey, cfg.Monitor,
		FastCRTSharpnessKey, cfg.FastCRTSharpness,
		ShowPerformanceKey, boolInt(cfg.ShowPerformance),
		DataDirectoryKey, cfg.DataDirectory,
	)
}

func cfgFileRead(cfg *TConfig) {
	cfgLock.Lock()
	defer cfgLock.Unlock()

	path := cfgFullPath()
	f, err := os.Open(path)
	if err != nil {
		if !os.IsNotExist(err) {
			fmt.Fprintf(os.Stderr, "WARN: failed to read shared settings %s: %v\n", path, err)
		}
		return
	}
	cfgActivePath = path

	defer func() {
		_ = f.Close()
	}()

	scanner := bufio.NewScanner(f)

	for scanner.Scan() {
		if err := cfgApplyLine(cfg, scanner.Text()); err != nil {
			fmt.Fprintln(os.Stderr, "failed to parse config:", err)
		}
	}

	if err := scanner.Err(); err != nil {
		fmt.Fprintln(os.Stderr, "reading standard input:", err)
	}
}

func cfgApplyLine(cfg *TConfig, line string) error {
	parseInteger := func(key string) (int, error) {
		value, err := strconv.Atoi(strings.TrimSpace(line[len(key):]))
		if err != nil {
			return 0, fmt.Errorf("%s: %w", strings.TrimSuffix(key, "="), err)
		}
		return value, nil
	}

	switch {
	case strings.HasPrefix(line, CurrentDayKey):
		value, err := parseInteger(CurrentDayKey)
		if err != nil {
			return err
		}
		cfg.CurrentDay = value
	case strings.HasPrefix(line, DateKey):
		value, err := parseInteger(DateKey)
		if err != nil {
			return err
		}
		cfg.CurrentDate = value
	case strings.HasPrefix(line, CRTFilterKey):
		cfg.CRTFilterEnabled = parseConfigBool(line[len(CRTFilterKey):])
	case strings.HasPrefix(line, CRTModeKey):
		cfg.CRTMode = strings.TrimSpace(line[len(CRTModeKey):])
	case strings.HasPrefix(line, SmoothingKey):
		cfg.SmoothingEnabled = parseConfigBool(line[len(SmoothingKey):])
	case strings.HasPrefix(line, ScalingModeKey):
		cfg.ScalingMode = strings.TrimSpace(line[len(ScalingModeKey):])
	case strings.HasPrefix(line, SceneOrderKey):
		cfg.ScenesInOrder = parseConfigBool(line[len(SceneOrderKey):])
	case strings.HasPrefix(line, WindowedKey):
		cfg.Windowed = parseConfigBool(line[len(WindowedKey):])
	case strings.HasPrefix(line, MuteKey):
		cfg.Mute = parseConfigBool(line[len(MuteKey):])
	case strings.HasPrefix(line, StretchKey):
		cfg.Stretch = parseConfigBool(line[len(StretchKey):])
	case strings.HasPrefix(line, MonitorKey):
		value, err := parseInteger(MonitorKey)
		if err != nil {
			return err
		}
		cfg.Monitor = value
	case strings.HasPrefix(line, FastCRTSharpnessKey):
		value, err := parseInteger(FastCRTSharpnessKey)
		if err != nil {
			return err
		}
		cfg.FastCRTSharpness = value
	case strings.HasPrefix(line, ShowPerformanceKey):
		cfg.ShowPerformance = parseConfigBool(line[len(ShowPerformanceKey):])
	case strings.HasPrefix(line, DataDirectoryKey):
		cfg.DataDirectory = strings.TrimSpace(line[len(DataDirectoryKey):])
	}
	return nil
}

func loadPersistentSettings(args []string) {
	var cfg TConfig
	cfgFileRead(&cfg)
	crtFilterMode = parseCRTFilter(cfg.CRTMode, cfg.CRTFilterEnabled)
	if appSettings.crt != "" {
		crtFilterMode = crtFilter(appSettings.crt)
	}
	fastCRTSharpness = decodeFastCRTSharpness(cfg.FastCRTSharpness)
	performancePinned = cfg.ShowPerformance
	imageScalingMode = parseImageScalingMode(cfg.ScalingMode, cfg.SmoothingEnabled)
	if cfg.ScenesInOrder {
		storyMode = storyPlaybackInOrder
	} else {
		storyMode = storyPlaybackRandom
	}
	appSettings = mergePersistentAppOptions(appSettings, cfg, args)
	if appSettings.dataDir == "" {
		appSettings.dataDir = cfg.DataDirectory
	}
	if appSettings.dataDir == "" {
		appSettings.dataDir = defaultDataDirectory()
	}
}

func defaultDataDirectory() string {
	executablePath, executableErr := os.Executable()
	workingDirectory, workingErr := os.Getwd()
	if executableErr != nil {
		executablePath = ""
	}
	if workingErr != nil {
		workingDirectory = ""
	}
	if directory, err := appDataDir(); err == nil {
		managedDirectory := filepath.Join(directory, "scrantic")
		if validateDataDirectory(managedDirectory) == nil {
			return managedDirectory
		}
	}

	return chooseDefaultDataDirectory(executablePath, workingDirectory, func(directory string) bool {
		return validateDataDirectory(directory) == nil
	})
}

func chooseDefaultDataDirectory(executablePath, workingDirectory string, valid func(string) bool) string {
	const defaultFolderName = "scrantic"
	candidates := make([]string, 0, 6)
	addCandidate := func(directory string) {
		if directory == "" {
			return
		}
		directory = filepath.Clean(directory)
		for _, candidate := range candidates {
			if strings.EqualFold(candidate, directory) {
				return
			}
		}
		candidates = append(candidates, directory)
	}

	if executablePath != "" {
		executableDirectory := filepath.Dir(executablePath)
		addCandidate(filepath.Join(executableDirectory, defaultFolderName))
		addCandidate(filepath.Join(executableDirectory, "..", defaultFolderName))
		// Development builds may live in a project build folder while the locally
		// supplied data directory lives beside the project.
		addCandidate(filepath.Join(executableDirectory, "..", "..", defaultFolderName))
	}
	if workingDirectory != "" {
		addCandidate(filepath.Join(workingDirectory, defaultFolderName))
		addCandidate(filepath.Join(workingDirectory, "..", defaultFolderName))
	}

	for _, candidate := range candidates {
		if valid(candidate) {
			return candidate
		}
	}
	if len(candidates) != 0 {
		// Keep startup errors and the folder picker pointed at the portable
		// convention even when the user has not copied the archives yet.
		return candidates[0]
	}
	return defaultFolderName
}

func mergePersistentAppOptions(options appOptions, cfg TConfig, args []string) appOptions {
	if !options.screenSaver && !optionWasProvided(args, "windowed", "fullscreen") {
		options.windowed = cfg.Windowed
	}
	if !optionWasProvided(args, "mute", "sound") {
		options.mute = cfg.Mute
	}
	if !optionWasProvided(args, "stretch", "fit") {
		options.stretch = cfg.Stretch
	}
	if !optionWasProvided(args, "monitor") && cfg.Monitor >= 1 {
		options.monitor = cfg.Monitor
	}
	return options
}

func persistDisplayPlaybackSettings() {
	if appSettings.noSaveSettings {
		return
	}
	var cfg TConfig
	cfgFileRead(&cfg)
	cfg.CRTFilterEnabled = crtFilterMode != crtOff
	cfg.CRTMode = string(crtFilterMode)
	cfg.FastCRTSharpness = encodeFastCRTSharpness(fastCRTSharpness)
	cfg.ShowPerformance = performancePinned
	cfg.SmoothingEnabled = imageScalingMode == scalingBilinear
	cfg.ScalingMode = string(imageScalingMode)
	cfg.ScenesInOrder = storyMode == storyPlaybackInOrder
	if !appSettings.screenSaver {
		cfg.Windowed = appSettings.windowed
		cfg.Mute = appSettings.mute
		cfg.Stretch = appSettings.stretch
		cfg.Monitor = appSettings.monitor
	}
	cfg.DataDirectory = appSettings.dataDir
	cfgFileWrite(&cfg)
}

func persistDataDirectory(directory string) {
	if appSettings.noSaveSettings {
		return
	}
	var cfg TConfig
	cfgFileRead(&cfg)
	cfg.DataDirectory = directory
	cfgFileWrite(&cfg)
}

func optionWasProvided(args []string, names ...string) bool {
	for _, arg := range args {
		option := strings.TrimLeft(strings.SplitN(arg, "=", 2)[0], "-")
		for _, name := range names {
			if option == name {
				return true
			}
		}
	}
	return false
}

func boolInt(value bool) int {
	if value {
		return 1
	}
	return 0
}

func parseConfigBool(value string) bool {
	parsed, err := strconv.ParseBool(strings.TrimSpace(value))
	if err == nil {
		return parsed
	}
	integer, err := strconv.Atoi(strings.TrimSpace(value))
	return err == nil && integer != 0
}
