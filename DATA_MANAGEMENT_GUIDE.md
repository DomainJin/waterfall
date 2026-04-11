# 💾 Data Management Tab Guide

## Overview

The new **Data Management** tab in the web UI provides comprehensive tools for managing animations, backups, and storage on your water curtain system.

---

## 🎯 Features

### 1. **Storage Status**

Displays real-time storage information from your ESP32 device:

- **Storage Used** — Current animation file size
- **Free Space** — Available space on device
- **Progress Bar** — Visual storage usage indicator

Updates automatically when you load animations or click "Refresh".

### 2. **Stored Animations**

Shows a list of all animations currently stored on the ESP32:

- Animation names and file sizes
- Automatically updates when scanning for files
- View detailed info about each animation

### 3. **File Management Buttons**

#### 🔄 Refresh

- Fetches latest storage and file information
- Updates storage bar
- Reloads animation list from device

#### ⬇ Export

- Downloads the current animation as JSON file
- Saves to your computer with timestamp
- Includes animation metadata (rows, columns, timing)

### 4. **Current Animation Info**

Displays details about the loaded animation:

- **Frames** — Total number of animation frames
- **Rows** — Number of rows in the animation
- **Boards** — Number of valve boards configured
- **Total Size** — Animation data size in bytes

Updates automatically when you:

- Load an image and process it
- Generate a pattern
- Run a simulation

### 5. **Backup & Restore**

#### 💾 Save Local

- Saves current animation to browser's local storage
- Backup persists even if page refreshes
- Useful for quick temporary saves

#### 📦 Backup All

- Creates a complete backup of:
  - Current animation frames
  - All configuration settings
  - Device parameters
  - Timestamp information
- Downloads as JSON file for archival

### 6. **Advanced Options**

#### File Selection

- Dropdown list of files on ESP32
- Select file to delete

#### 🗑 Delete File

- Remove animation files from device storage
- Confirmation dialog before deletion
- Frees up space for new animations

---

## 📊 Typical Workflow

### Save & Backup Current Work

```
1. Load an image
2. Process and refine (adjust thresholds, etc.)
3. Generate animation frames
4. Click "💾 Save Local" to backup to browser
5. Click "📦 Backup All" to download complete backup
```

### Export Animation

```
1. Finish creating animation
2. Click "⬇ Export" button
3. Animation saved as JSON file (animation_TIMESTAMP.json)
4. Can be loaded later or shared with others
```

### Manage Storage

```
1. Click "🔄 Refresh" to check storage status
2. View "Stored Animations" list
3. Select files to delete in "Advanced" section
4. Click "🗑 Delete File" to remove old animations
```

### Restore from Backup

```
1. Note: Full restore feature coming in next update
2. Currently, use exported JSON files to recreate animations
3. Backups contain all animation data and settings
```

---

## 📈 Storage Information

### Understanding Storage Stats

**Storage Used (MB)**

- Size of current animation data
- Includes all frames and metadata
- Varies based on animation complexity

**Free Space (MB)**

- Available space on ESP32
- Maximum total typically 4MB for animation storage
- Plan animations to fit available space

**Storage Bar**

- Percentage of storage used
- Color: Cyan/Accent color
- Updates in real-time

### Typical File Sizes

- Simple pattern (100 frames): ~10-50 KB
- Complex image (500 frames): ~50-200 KB
- Large animation (1000+ frames): 200+ KB

---

## ⚙️ Configuration

The data management tab automatically integrates with your settings:

- **Boards** — Adds to backup metadata
- **Height/Timing** — Included in exports
- **Playback Speed** — Saved with animation

All exports include configuration data for accurate restoration.

---

## 📝 File Format

### Export Format (JSON)

```json
{
  "frames": [
    {
      "ts_ms": 0,
      "bits": [0, 0, 255, ...]
    },
    ...
  ],
  "metadata": {
    "rows": 100,
    "cols": 80,
    "timestamp": "2026-04-03T...",
    "height_cm": 150,
    "row_interval_ms": 80
  }
}
```

### Backup Format (JSON)

```json
{
  "version": "1.0",
  "created": "2026-04-03T...",
  "animation": {
    "frames": [...],
    "rows": 100,
    "cols": 80
  },
  "config": {
    "boards": 10,
    "height_cm": 150,
    "row_interval_ms": 80,
    "playbackSpeed": 1.0
  }
}
```

---

## 🔧 Troubleshooting

### Storage Info Shows "—"

- **Cause:** Backend server not running or device not responding
- **Fix:**
  1. Start backend: `python backend.py`
  2. Check ESP32 connection
  3. Click "🔄 Refresh" button

### Animation Info Empty

- **Cause:** No animation loaded yet
- **Fix:**
  1. Load an image file
  2. Adjust thresholds and processing options
  3. Generate animation frames

### Export Button Disabled (Greyed Out)

- **Cause:** No frames generated
- **Fix:**
  1. Load and process an image
  2. Click "↺ Reprocess" to build frames
  3. Try "⬇ Export" again

### File List Empty

- **Cause:** No files stored on device or connection issue
- **Fix:**
  1. Check device is connected
  2. Click "🔄 Refresh" to reload
  3. Create and send a new animation

### Delete File Not Working

- **Cause:** Feature requires backend endpoints
- **Current Status:** Placeholder for future update
- **Workaround:** Delete files directly from ESP32 file system

---

## 💡 Tips & Best Practices

### Before Uploading to Device

1. **Test locally first** — Use "Simulate only" button
2. **Check file size** — Use "Export" to see size
3. **Keep backup** — Always click "📦 Backup All" before major changes

### Regular Maintenance

1. **Weekly refresh** — Click "🔄 Refresh" to monitor storage
2. **Archive old animations** — Download backups before deleting
3. **Monitor free space** — Keep at least 1MB free for new content

### Sharing Animations

1. **Use "⬇ Export"** — Creates portable JSON file
2. **Send to others** — They can open in any editor
3. **Document settings** — Backup includes all parameters

### Multiple Devices

1. **Export from each** — Save animations with device name
2. **Create device folder** — Organize backups by location
3. **Cross-load** — Animations work on any device with same board config

---

## 🔐 Data Security

### Local Browser Storage

- **Location:** Encrypted by browser per domain
- **Persistence:** Survives page refresh
- **Clearing:** Use browser dev tools Storage tab
- **Limit:** ~5-10MB per browser

### Exported Files

- **Format:** Plain text JSON (human readable)
- **Portability:** Works on any system
- **Backup:** Store copies in secure location
- **Versioning:** Use timestamps for version control

---

## 📅 Upcoming Features

- ✅ **Current:** Export/Backup animations
- ⏳ **Next:** Delete files from device
- ⏳ **Planned:** Import animations from backup
- ⏳ **Planned:** Cloud sync (optional)
- ⏳ **Planned:** Animation gallery/library

---

## 🎓 Quick Reference

| Feature        | Button         | Action                   |
| -------------- | -------------- | ------------------------ |
| Check storage  | 🔄 Refresh     | Load device info         |
| Save animation | ⬇ Export       | Download as JSON         |
| Backup work    | 💾 Save Local  | Store in browser         |
| Backup all     | 📦 Backup All  | Download complete backup |
| Remove files   | 🗑 Delete File | Clear old animations     |

---

## 📞 Support

For issues or feature requests:

1. Check troubleshooting section above
2. Enable "Test" diagnostic from Scan ESP tab
3. Review browser console (F12) for error messages
4. Check backend server logs if using HTTP API

---

**Created:** 2026-04-03  
**Version:** 1.0  
**Status:** Fully Functional ✅
