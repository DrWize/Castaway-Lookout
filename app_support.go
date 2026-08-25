package main

import (
	"log"
	"os"
	"path/filepath"
	"runtime/debug"
)

const (
	appDirectoryName = "JohnnyCastaway"
	appVersion       = "2026.1.0-rc.3"
	appLogName       = "JohnnyCastaway.log"
	appLogMaxSize    = 1 << 20
)

func appDataDir() (string, error) {
	base, err := os.UserCacheDir()
	if err != nil {
		return "", err
	}
	return filepath.Join(base, appDirectoryName), nil
}

func initAppLogging() (func(), error) {
	dir, err := appDataDir()
	if err != nil {
		return func() {}, err
	}
	if err := os.MkdirAll(dir, 0755); err != nil {
		return func() {}, err
	}
	logPath := filepath.Join(dir, appLogName)
	if err := rotateLogIfNeeded(logPath, appLogMaxSize); err != nil {
		return func() {}, err
	}
	file, err := os.OpenFile(logPath, os.O_CREATE|os.O_APPEND|os.O_WRONLY, 0644)
	if err != nil {
		return func() {}, err
	}
	log.SetOutput(file)
	log.SetFlags(log.Ldate | log.Ltime | log.Lmicroseconds)
	log.Printf("---------------- new session ----------------")
	log.Printf("Johnny Castaway %s (build %s)", appVersion, appBuildIdentifier())
	return func() { _ = file.Close() }, nil
}

func rotateLogIfNeeded(path string, maxSize int64) error {
	info, err := os.Stat(path)
	if os.IsNotExist(err) {
		return nil
	}
	if err != nil {
		return err
	}
	if info.Size() < maxSize {
		return nil
	}

	backupPath := path + ".1"
	if err := os.Remove(backupPath); err != nil && !os.IsNotExist(err) {
		return err
	}
	return os.Rename(path, backupPath)
}

func appBuildIdentifier() string {
	info, ok := debug.ReadBuildInfo()
	if !ok {
		return "development"
	}

	revision := "development"
	modified := false
	for _, setting := range info.Settings {
		switch setting.Key {
		case "vcs.revision":
			revision = setting.Value
			if len(revision) > 12 {
				revision = revision[:12]
			}
		case "vcs.modified":
			modified = setting.Value == "true"
		}
	}
	if modified {
		revision += "-dirty"
	}
	return revision
}

func appVersionLabel() string {
	return "v" + appVersion + " / " + appBuildIdentifier()
}
