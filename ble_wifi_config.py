#!/usr/bin/env python3
"""
ESP32 BLE WiFi Config Tool
Quét BLE, kết nối Waterfall_Config, cấu hình WiFi

Cài đặt:
    pip install bleak

Chạy:
    python ble_wifi_config.py
"""

import asyncio
import threading
import tkinter as tk
from tkinter import ttk, messagebox, scrolledtext

from bleak import BleakScanner, BleakClient, BleakError

# ── UUIDs (phải khớp với ble_config.h) ────────────────────────────────────────
SERVICE_UUID     = "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
CHAR_WIFI_UUID   = "beb5483e-36e1-4688-b7f5-ea07361b26a8"  # WRITE
CHAR_STATUS_UUID = "beb5483e-36e1-4688-b7f5-ea07361b26a9"  # READ
CHAR_INFO_UUID   = "beb5483e-36e1-4688-b7f5-ea07361b26aa"  # READ
DEVICE_NAME      = "Waterfall_Config"
SCAN_TIMEOUT     = 8.0  # giây


# ── Async helper ───────────────────────────────────────────────────────────────
def _run_async(coro, callback=None):
    """Chạy coroutine trong background thread, gọi callback(result, error) khi xong."""
    def runner():
        try:
            result = asyncio.run(coro)
            if callback:
                callback(result, None)
        except Exception as e:
            if callback:
                callback(None, e)
    threading.Thread(target=runner, daemon=True).start()


# ── BLE operations ─────────────────────────────────────────────────────────────
async def ble_scan(progress_cb=None):
    """Quét BLE, trả về list (name, address, rssi)."""
    if progress_cb:
        progress_cb("Đang quét BLE...")

    # return_adv=True cho bleak >= 0.20 — trả về dict {addr: (BLEDevice, AdvertisementData)}
    devices_adv = await BleakScanner.discover(timeout=SCAN_TIMEOUT, return_adv=True)
    result = []
    for addr, (device, adv) in devices_adv.items():
        name = device.name or adv.local_name or "(no name)"
        rssi = adv.rssi if adv.rssi is not None else "?"
        result.append((name, addr, rssi))

    result.sort(key=lambda x: (x[0] != DEVICE_NAME, x[0]))  # Waterfall_Config lên đầu
    return result


async def ble_read_info(address):
    """Kết nối và đọc device info + WiFi status."""
    async with BleakClient(address, timeout=10) as client:
        info_bytes   = await client.read_gatt_char(CHAR_INFO_UUID)
        status_bytes = await client.read_gatt_char(CHAR_STATUS_UUID)
        return {
            "info":   info_bytes.decode("utf-8", errors="replace"),
            "status": status_bytes.decode("utf-8", errors="replace"),
        }


async def ble_send_wifi(address, ssid, password):
    """Kết nối và gửi WiFi credentials theo format SSID|PASSWORD."""
    payload = f"{ssid}|{password}".encode("utf-8")
    async with BleakClient(address, timeout=10) as client:
        await client.write_gatt_char(CHAR_WIFI_UUID, payload, response=True)
    return True


# ── GUI ────────────────────────────────────────────────────────────────────────
class App:
    def __init__(self, root: tk.Tk):
        self.root = root
        self.root.title("ESP32 BLE WiFi Config")
        self.root.geometry("640x560")
        self.root.resizable(False, False)

        self._selected_address = None
        self._selected_name    = None
        self._build_ui()

    # ── UI builder ─────────────────────────────────────────────────────────────
    def _build_ui(self):
        root = self.root
        PAD = 10

        # ── Header ─────────────────────────────────────────────────────────────
        hdr = tk.Frame(root, bg="#1565C0", height=48)
        hdr.pack(fill="x")
        tk.Label(hdr, text="  ESP32 BLE WiFi Config",
                 bg="#1565C0", fg="white",
                 font=("Segoe UI", 14, "bold")).pack(side="left", pady=8)

        # ── Scan frame ─────────────────────────────────────────────────────────
        scan_lf = ttk.LabelFrame(root, text="1. Quét thiết bị BLE", padding=PAD)
        scan_lf.pack(fill="x", padx=PAD, pady=(PAD, 4))

        scan_ctrl = tk.Frame(scan_lf)
        scan_ctrl.pack(fill="x")

        self.btn_scan = ttk.Button(scan_ctrl, text="Quét BLE",
                                   command=self._on_scan, width=14)
        self.btn_scan.pack(side="left")

        self.scan_status = tk.StringVar(value="Nhấn 'Quét BLE' để tìm thiết bị")
        ttk.Label(scan_ctrl, textvariable=self.scan_status,
                  foreground="#555").pack(side="left", padx=12)

        # Device list
        list_frame = tk.Frame(scan_lf)
        list_frame.pack(fill="both", expand=True, pady=(8, 0))

        sb = ttk.Scrollbar(list_frame)
        sb.pack(side="right", fill="y")

        self.device_list = tk.Listbox(list_frame, yscrollcommand=sb.set,
                                      font=("Consolas", 10), height=7,
                                      selectbackground="#1565C0",
                                      selectforeground="white",
                                      activestyle="none")
        self.device_list.pack(fill="both", expand=True)
        sb.config(command=self.device_list.yview)
        self.device_list.bind("<<ListboxSelect>>", self._on_device_select)

        # ── Info frame ─────────────────────────────────────────────────────────
        info_lf = ttk.LabelFrame(root, text="2. Thông tin thiết bị", padding=PAD)
        info_lf.pack(fill="x", padx=PAD, pady=4)

        info_row = tk.Frame(info_lf)
        info_row.pack(fill="x")

        self.selected_var = tk.StringVar(value="(chưa chọn)")
        ttk.Label(info_row, text="Đã chọn:").pack(side="left")
        ttk.Label(info_row, textvariable=self.selected_var,
                  foreground="#1565C0",
                  font=("Segoe UI", 10, "bold")).pack(side="left", padx=6)

        self.btn_info = ttk.Button(info_row, text="Đọc thông tin",
                                   command=self._on_read_info, width=16)
        self.btn_info.pack(side="right")

        self.info_text = scrolledtext.ScrolledText(info_lf, height=3,
                                                    font=("Consolas", 9),
                                                    state="disabled",
                                                    background="#F5F5F5")
        self.info_text.pack(fill="x", pady=(6, 0))

        # ── WiFi config frame ──────────────────────────────────────────────────
        wifi_lf = ttk.LabelFrame(root, text="3. Cấu hình WiFi", padding=PAD)
        wifi_lf.pack(fill="x", padx=PAD, pady=4)

        grid = tk.Frame(wifi_lf)
        grid.pack(fill="x")
        grid.columnconfigure(1, weight=1)

        ttk.Label(grid, text="SSID:").grid(row=0, column=0, sticky="w", pady=5)
        self.ssid_var = tk.StringVar()
        ttk.Entry(grid, textvariable=self.ssid_var,
                  font=("Segoe UI", 11)).grid(row=0, column=1, sticky="ew", padx=(8, 0))

        ttk.Label(grid, text="Password:").grid(row=1, column=0, sticky="w", pady=5)
        self.pw_var = tk.StringVar()
        self.pw_entry = ttk.Entry(grid, textvariable=self.pw_var,
                                   font=("Segoe UI", 11), show="*")
        self.pw_entry.grid(row=1, column=1, sticky="ew", padx=(8, 0))

        # Show/hide password
        self.show_pw = tk.BooleanVar(value=False)
        ttk.Checkbutton(wifi_lf, text="Hiện mật khẩu",
                        variable=self.show_pw,
                        command=self._toggle_pw).pack(anchor="e")

        self.btn_send = ttk.Button(wifi_lf, text="Gửi WiFi → ESP32",
                                    command=self._on_send_wifi)
        self.btn_send.pack(fill="x", pady=(6, 0))

        # ── Log ────────────────────────────────────────────────────────────────
        log_lf = ttk.LabelFrame(root, text="Log", padding=(PAD, 4))
        log_lf.pack(fill="both", expand=True, padx=PAD, pady=(4, PAD))

        self.log = scrolledtext.ScrolledText(log_lf, height=5,
                                              font=("Consolas", 9),
                                              state="disabled",
                                              background="#1E1E1E",
                                              foreground="#DCDCDC")
        self.log.pack(fill="both", expand=True)

        # Tag màu log
        self.log.tag_config("ok",    foreground="#4CAF50")
        self.log.tag_config("err",   foreground="#F44336")
        self.log.tag_config("info",  foreground="#64B5F6")
        self.log.tag_config("plain", foreground="#DCDCDC")

    # ── Helpers ────────────────────────────────────────────────────────────────
    def _log(self, msg, tag="plain"):
        def _do():
            self.log.config(state="normal")
            self.log.insert("end", msg + "\n", tag)
            self.log.see("end")
            self.log.config(state="disabled")
        self.root.after(0, _do)

    def _set_info(self, text):
        def _do():
            self.info_text.config(state="normal")
            self.info_text.delete("1.0", "end")
            self.info_text.insert("1.0", text)
            self.info_text.config(state="disabled")
        self.root.after(0, _do)

    def _toggle_pw(self):
        self.pw_entry.config(show="" if self.show_pw.get() else "*")

    # ── Scan ───────────────────────────────────────────────────────────────────
    def _on_scan(self):
        self.btn_scan.config(state="disabled")
        self.device_list.delete(0, "end")
        self.scan_status.set(f"Đang quét ({int(SCAN_TIMEOUT)}s)...")
        self._log(f"[BLE] Bắt đầu quét ({int(SCAN_TIMEOUT)}s)...", "info")

        def done(result, error):
            def _do():
                self.btn_scan.config(state="normal")
                if error:
                    self.scan_status.set("Lỗi quét BLE")
                    self._log(f"[ERR] {error}", "err")
                    messagebox.showerror("Lỗi BLE", str(error))
                    return

                if not result:
                    self.scan_status.set("Không tìm thấy thiết bị")
                    self._log("[BLE] Không tìm thấy thiết bị nào", "err")
                    return

                self.scan_status.set(f"Tìm thấy {len(result)} thiết bị")
                self._log(f"[BLE] Tìm thấy {len(result)} thiết bị:", "ok")

                for name, addr, rssi in result:
                    marker = " ★" if name == DEVICE_NAME else ""
                    label = f"{name:<28} {addr}  {rssi} dBm{marker}"
                    self.device_list.insert("end", label)
                    tag = "ok" if name == DEVICE_NAME else "plain"
                    self._log(f"  {label}", tag)

                    # Tự động chọn Waterfall_Config nếu tìm thấy
                    if name == DEVICE_NAME:
                        idx = self.device_list.size() - 1
                        self.device_list.selection_set(idx)
                        self.device_list.see(idx)
                        self._selected_address = addr
                        self._selected_name    = name
                        self.selected_var.set(f"{name}  [{addr}]")

            self.root.after(0, _do)

        _run_async(ble_scan(), callback=done)

    # ── Select device ──────────────────────────────────────────────────────────
    def _on_device_select(self, _event=None):
        sel = self.device_list.curselection()
        if not sel:
            return
        line = self.device_list.get(sel[0])
        # địa chỉ MAC nằm ở cột thứ 2 (sau tên) — 17 ký tự, 5 dấu ':'
        parts = line.split()
        for p in parts:
            if len(p) == 17 and p.count(":") == 5:
                self._selected_address = p
                break
        # Lấy tên (cắt bỏ RSSI và ★)
        self._selected_name = line.split("  ")[0].strip() if "  " in line else line.strip()
        self.selected_var.set(f"{self._selected_name}  [{self._selected_address}]")
        self._log(f"[BLE] Đã chọn: {self._selected_address}", "info")

    # ── Read info ──────────────────────────────────────────────────────────────
    def _on_read_info(self):
        if not self._selected_address:
            messagebox.showwarning("Chưa chọn thiết bị", "Hãy quét và chọn ESP32 trước.")
            return

        self.btn_info.config(state="disabled")
        self._log(f"[BLE] Đang kết nối {self._selected_address}...", "info")

        def done(result, error):
            def _do():
                self.btn_info.config(state="normal")
                if error:
                    self._log(f"[ERR] Không đọc được info: {error}", "err")
                    messagebox.showerror("Lỗi kết nối", str(error))
                    return
                info   = result.get("info", "")
                status = result.get("status", "")
                display = f"Info:   {info}\nStatus: {status}"
                self._set_info(display)
                self._log(f"[BLE] Info:   {info}", "ok")
                self._log(f"[BLE] Status: {status}", "ok")
            self.root.after(0, _do)

        _run_async(ble_read_info(self._selected_address), callback=done)

    # ── Send WiFi ──────────────────────────────────────────────────────────────
    def _on_send_wifi(self):
        if not self._selected_address:
            messagebox.showwarning("Chưa chọn thiết bị", "Hãy quét và chọn ESP32 trước.")
            return

        ssid = self.ssid_var.get().strip()
        pw   = self.pw_var.get()

        if not ssid:
            messagebox.showwarning("Thiếu SSID", "Nhập tên WiFi (SSID) trước.")
            return

        confirm = messagebox.askyesno(
            "Xác nhận",
            f"Gửi WiFi đến ESP32?\n\nSSID:     {ssid}\nPassword: {'*' * len(pw)}\nAddress:  {self._selected_address}"
        )
        if not confirm:
            return

        self.btn_send.config(state="disabled")
        self._log(f"[BLE] Gửi WiFi '{ssid}' → {self._selected_address}...", "info")

        def done(result, error):
            def _do():
                self.btn_send.config(state="normal")
                if error:
                    self._log(f"[ERR] Gửi thất bại: {error}", "err")
                    messagebox.showerror("Gửi thất bại", str(error))
                    return
                self._log(f"[BLE] Gửi WiFi thành công! ESP32 đang kết nối...", "ok")
                messagebox.showinfo(
                    "Thành công",
                    f"Đã gửi WiFi cho ESP32.\n\n"
                    f"ESP32 sẽ tự kết nối WiFi '{ssid}'.\n"
                    f"Dùng 'Đọc thông tin' để xem IP sau vài giây."
                )
            self.root.after(0, _do)

        _run_async(ble_send_wifi(self._selected_address, ssid, pw), callback=done)


# ── Entry point ────────────────────────────────────────────────────────────────
def main():
    try:
        import bleak  # noqa: F401
    except ImportError:
        print("Lỗi: bleak chưa được cài.\nChạy: pip install bleak")
        input("Nhấn Enter để thoát...")
        return

    root = tk.Tk()

    # Kiểu dáng
    style = ttk.Style(root)
    try:
        style.theme_use("vista")
    except Exception:
        pass

    App(root)
    root.mainloop()


if __name__ == "__main__":
    main()
