#!/usr/bin/env python3
"""
Water Curtain ESP32 — UDP Config Tool
Đổi cấu hình (IP, Port, mode) không cần upload lại firmware

Usage:
  python set_config.py                          # interactive / GUI
  python set_config.py --esp 192.168.1.241      # xem info
  python set_config.py --esp 192.168.1.241 --ip 192.168.1.100  # set IP
  python set_config.py --scan                   # scan all ESP32 devices
  python set_config.py --gui                    # mở GUI
"""

import socket
import sys
import argparse
import tkinter as tk
from tkinter import ttk, messagebox, scrolledtext
import threading
import ipaddress
from concurrent.futures import ThreadPoolExecutor, as_completed

# Configuration
CFG_PORT   = 8888
TIMEOUT    = 1.0  # Timeout for UDP (increased for reliability, was 0.8)
TIMEOUT_QUICK = 0.5  # First attempt quick timeout
ESP32_DEFAULT_IP = "192.168.1.241"

def get_local_network():
    """Get local network IP range"""
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.connect(("8.8.8.8", 80))
        local_ip = s.getsockname()[0]
        s.close()
        
        parts = local_ip.split('.')
        return f"{parts[0]}.{parts[1]}.{parts[2]}"
    except:
        return "192.168.1"


def send_udp_cmd(esp_ip, cmd, timeout=TIMEOUT):
    """Send UDP command to ESP32 and get reply"""
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.settimeout(timeout)
    try:
        sock.sendto(cmd.encode(), (esp_ip, CFG_PORT))
        reply, addr = sock.recvfrom(512)
        return reply.decode().strip()
    except socket.timeout:
        return None
    except Exception as e:
        return f"ERROR: {e}"
    finally:
        sock.close()


def is_esp32_available(ip_str):
    """Check if ESP32 is available at IP (two-stage: quick then retry)"""
    try:
        # Stage 1: Quick check with short timeout
        r = send_udp_cmd(ip_str, "GET_INFO", timeout=TIMEOUT_QUICK)
        if r is not None and not r.startswith("ERROR"):
            return True
        
        # Stage 2: Retry with longer timeout (in case ESP32 was busy)
        r = send_udp_cmd(ip_str, "GET_INFO", timeout=TIMEOUT)
        return r is not None and not r.startswith("ERROR")
    except:
        return False


def scan_for_devices(network_prefix="192.168.1", callback=None, max_workers=50):
    """Parallel scan for ESP32 devices"""
    found = []
    
    def check_ip(ip):
        if is_esp32_available(ip):
            return ip
        return None
    
    with ThreadPoolExecutor(max_workers=max_workers) as executor:
        futures = {executor.submit(check_ip, f"{network_prefix}.{i}"): i for i in range(1, 255)}
        
        for future in as_completed(futures):
            result = future.result()
            if result:
                found.append(result)
                if callback:
                    callback(result)
    
    return sorted(found)


def cmd_info(esp_ip):
    """Get device info"""
    print(f"[*] Getting info from {esp_ip}...")
    r = send_udp_cmd(esp_ip, "GET_INFO")
    if r:
        print(f"[OK] {r}")
        return r
    else:
        print(f"[ERR] No response from {esp_ip}:{CFG_PORT}")
        return None


def cmd_set_ip(esp_ip, new_ip):
    """Set remote target IP"""
    print(f"[*] Setting IP on {esp_ip} → {new_ip}")
    r = send_udp_cmd(esp_ip, f"SET_IP:{new_ip}")
    if r and r.startswith("OK:"):
        print(f"[OK] {r}")
        return True
    else:
        print(f"[ERR] {r or 'timeout'}")
        return False


def cmd_set_port(esp_ip, new_port):
    """Set remote target port"""
    print(f"[*] Setting port on {esp_ip} → {new_port}")
    r = send_udp_cmd(esp_ip, f"SET_PORT:{new_port}")
    if r and r.startswith("OK:"):
        print(f"[OK] {r}")
        return True
    else:
        print(f"[ERR] {r or 'timeout'}")
        return False


def cmd_reset(esp_ip):
    """Reset ESP32"""
    print(f"[*] Sending RESET to {esp_ip}...")
    r = send_udp_cmd(esp_ip, "RESET", timeout=2)
    if r and r.startswith("OK:"):
        print(f"[OK] {r}")
        return True
    else:
        print(f"[ERR] {r or 'timeout'}")
        return False


def interactive_cli(esp_ip):
    """Interactive CLI mode"""
    print(f"\n{'='*60}")
    print(f"Water Curtain ESP32 Config Tool")
    print(f"Target: {esp_ip}:{CFG_PORT}")
    print(f"{'='*60}")
    
    # Show current info
    cmd_info(esp_ip)
    
    print("\nCommands:")
    print("  info              - Get device info")
    print("  set-ip <IP>       - Set remote IP")
    print("  set-port <PORT>   - Set remote port")
    print("  reset             - Restart ESP32")
    print("  scan [NETWORK]    - Scan for devices (e.g: 192.168.1)")
    print("  quit              - Exit\n")
    
    while True:
        try:
            line = input("> ").strip()
        except (EOFError, KeyboardInterrupt):
            break
        
        if not line:
            continue
        elif line == "info":
            cmd_info(esp_ip)
        elif line.startswith("set-ip "):
            new_ip = line[7:].strip()
            cmd_set_ip(esp_ip, new_ip)
        elif line.startswith("set-port "):
            try:
                port = int(line[9:].strip())
                cmd_set_port(esp_ip, port)
            except ValueError:
                print("[ERR] Invalid port number")
        elif line == "reset":
            if messagebox.askyesno("Confirm", "Reset ESP32? It will restart."):
                cmd_reset(esp_ip)
        elif line.startswith("scan"):
            parts = line.split()
            net = parts[1] if len(parts) > 1 else get_local_network()
            print(f"\n[*] Scanning {net}.* ...")
            devices = scan_for_devices(net)
            if devices:
                print(f"[OK] Found {len(devices)} device(s):")
                for ip in devices:
                    print(f"  • {ip}")
            else:
                print("[!] No devices found")
        elif line in ("quit", "exit", "q"):
            break
        else:
            print("[?] Unknown command. Try: info, set-ip, set-port, reset, scan, quit")


class ConfigGUI:
    def __init__(self, root):
        self.root = root
        self.esp_ip = ESP32_DEFAULT_IP
        self.found_devices = []
        
        self.root.title("Water Curtain ESP32 - Config Tool")
        self.root.geometry("700x600")
        
        pad = 10
        
        # Title
        title = ttk.Label(root, text="🎭 Water Curtain ESP32 Configuration", 
                         font=("Arial", 14, "bold"))
        title.pack(pady=(pad, 5), padx=pad)
        
        # Create tabs
        notebook = ttk.Notebook(root)
        notebook.pack(fill="both", expand=True, padx=pad, pady=5)
        
        # Tab 1: Scan
        self.scan_tab = ttk.Frame(notebook)
        notebook.add(self.scan_tab, text="🔍 Scan Devices")
        self._create_scan_tab()
        
        # Tab 2: Config
        self.config_tab = ttk.Frame(notebook)
        notebook.add(self.config_tab, text="⚙️  Configuration")
        self._create_config_tab()
        
        # Status bar
        self.status_var = tk.StringVar(value="Ready")
        status = ttk.Label(root, textvariable=self.status_var, 
                          font=("Arial", 9), background="lightgray", relief="sunken")
        status.pack(fill="x", side="bottom")
    
    def _create_scan_tab(self):
        frame = ttk.Frame(self.scan_tab, padding=10)
        frame.pack(fill="both", expand=True)
        
        # Scan controls
        ctrl_frame = ttk.Frame(frame)
        ctrl_frame.pack(fill="x", pady=(0, 10))
        
        ttk.Button(ctrl_frame, text="🔍 Scan Network", command=self._on_scan).pack(side="left", padx=3)
        ttk.Label(ctrl_frame, text="Network:").pack(side="left", padx=(15, 5))
        
        self.network_var = tk.StringVar(value=get_local_network())
        net_entry = ttk.Entry(ctrl_frame, textvariable=self.network_var, width=15)
        net_entry.pack(side="left")
        
        # Device list
        ttk.Label(frame, text="Available Devices:", font=("Arial", 10, "bold")).pack(anchor="w", pady=(10, 5))
        
        scroll = ttk.Scrollbar(frame)
        scroll.pack(side="right", fill="y")
        
        self.device_list = tk.Listbox(frame, yscrollcommand=scroll.set, font=("Courier", 11), height=15)
        self.device_list.pack(fill="both", expand=True)
        scroll.config(command=self.device_list.yview)
        self.device_list.bind("<<ListboxSelect>>", self._on_device_select)
        
        # Select button
        ttk.Button(frame, text="✓ Select Device", command=self._on_select_device).pack(pady=(10, 0), fill="x")
    
    def _create_config_tab(self):
        frame = ttk.Frame(self.config_tab, padding=10)
        frame.pack(fill="both", expand=True)
        
        # ESP32 IP
        ttk.Label(frame, text="ESP32 IP:", font=("Arial", 11, "bold")).grid(row=0, column=0, sticky="w", pady=10)
        self.esp_ip_var = tk.StringVar(value=self.esp_ip)
        esp_entry = ttk.Entry(frame, textvariable=self.esp_ip_var, width=30, font=("Arial", 10))
        esp_entry.grid(row=0, column=1, sticky="ew", padx=(10, 0))
        
        # Info display
        ttk.Label(frame, text="Device Info:", font=("Arial", 11, "bold")).grid(row=1, column=0, sticky="nw", pady=(15, 5))
        self.info_text = scrolledtext.ScrolledText(frame, height=5, width=40, state="disabled", font=("Courier", 9))
        self.info_text.grid(row=1, column=1, sticky="ew", padx=(10, 0))
        
        ttk.Button(frame, text="🔄 Refresh Info", command=self._on_refresh_info).grid(row=2, column=1, sticky="w", pady=(10, 0))
        
        # Separator
        sep = ttk.Separator(frame, orient="horizontal")
        sep.grid(row=3, column=0, columnspan=2, sticky="ew", pady=15)
        
        # Set IP
        ttk.Label(frame, text="Remote IP:", font=("Arial", 11, "bold")).grid(row=4, column=0, sticky="w", pady=10)
        self.new_ip_var = tk.StringVar(value="192.168.1.100")
        ip_entry = ttk.Entry(frame, textvariable=self.new_ip_var, width=30, font=("Arial", 10))
        ip_entry.grid(row=4, column=1, sticky="ew", padx=(10, 0))
        
        ttk.Button(frame, text="✓ Set IP", command=self._on_set_ip).grid(row=5, column=1, sticky="ew", pady=(5, 0))
        
        # Set Port
        ttk.Label(frame, text="Remote Port:", font=("Arial", 11, "bold")).grid(row=6, column=0, sticky="w", pady=10)
        self.new_port_var = tk.StringVar(value="3333")
        port_entry = ttk.Entry(frame, textvariable=self.new_port_var, width=30, font=("Arial", 10))
        port_entry.grid(row=6, column=1, sticky="ew", padx=(10, 0))
        
        ttk.Button(frame, text="✓ Set Port", command=self._on_set_port).grid(row=7, column=1, sticky="ew", pady=(5, 0))
        
        # Reset button
        ttk.Button(frame, text="⚠️  Reset ESP32", command=self._on_reset).grid(row=8, column=1, sticky="ew", pady=(15, 0))
        
        frame.columnconfigure(1, weight=1)
    
    def _on_device_select(self, event):
        sel = self.device_list.curselection()
        if sel:
            ip = self.device_list.get(sel[0])
            self.esp_ip_var.set(ip)
    
    def _on_select_device(self):
        sel = self.device_list.curselection()
        if sel:
            ip = self.device_list.get(sel[0])
            self.esp_ip_var.set(ip)
            self._on_refresh_info()
            messagebox.showinfo("Selected", f"Device {ip} selected")
        else:
            messagebox.showwarning("Warning", "Select a device first")
    
    def _on_scan(self):
        def scan():
            self.status_var.set("Scanning...")
            self.device_list.delete(0, tk.END)
            
            net = self.network_var.get()
            devices = scan_for_devices(net, callback=lambda ip: self.device_list.insert(tk.END, ip))
            
            if devices:
                self.status_var.set(f"Found {len(devices)} device(s)")
            else:
                self.status_var.set("No devices found")
        
        threading.Thread(target=scan, daemon=True).start()
    
    def _on_refresh_info(self):
        def fetch():
            esp_ip = self.esp_ip_var.get()
            self.status_var.set("Fetching info...")
            r = send_udp_cmd(esp_ip, "GET_INFO")
            if r and not r.startswith("ERROR"):
                self.info_text.config(state="normal")
                self.info_text.delete("1.0", "end")
                self.info_text.insert("1.0", r)
                self.info_text.config(state="disabled")
                self.status_var.set("Info loaded")
            else:
                messagebox.showerror("Error", f"Cannot reach {esp_ip}:{CFG_PORT}")
                self.status_var.set("Connection failed")
        
        threading.Thread(target=fetch, daemon=True).start()
    
    def _on_set_ip(self):
        def set_ip():
            esp_ip = self.esp_ip_var.get()
            new_ip = self.new_ip_var.get()
            self.status_var.set(f"Setting IP to {new_ip}...")
            r = send_udp_cmd(esp_ip, f"SET_IP:{new_ip}")
            if r and r.startswith("OK:"):
                self.status_var.set("IP set successfully")
                messagebox.showinfo("Success", f"Remote IP set to {new_ip}")
                self._on_refresh_info()
            else:
                messagebox.showerror("Error", f"Failed: {r or 'timeout'}")
                self.status_var.set("Failed to set IP")
        
        threading.Thread(target=set_ip, daemon=True).start()
    
    def _on_set_port(self):
        def set_port():
            esp_ip = self.esp_ip_var.get()
            try:
                new_port = int(self.new_port_var.get())
            except ValueError:
                messagebox.showerror("Error", "Invalid port number")
                return
            
            self.status_var.set(f"Setting port to {new_port}...")
            r = send_udp_cmd(esp_ip, f"SET_PORT:{new_port}")
            if r and r.startswith("OK:"):
                self.status_var.set("Port set successfully")
                messagebox.showinfo("Success", f"Remote port set to {new_port}")
                self._on_refresh_info()
            else:
                messagebox.showerror("Error", f"Failed: {r or 'timeout'}")
                self.status_var.set("Failed to set port")
        
        threading.Thread(target=set_port, daemon=True).start()
    
    def _on_reset(self):
        if messagebox.askyesno("Confirm", "Reset ESP32? It will restart.\n\nContinue?"):
            esp_ip = self.esp_ip_var.get()
            threading.Thread(target=lambda: cmd_reset(esp_ip), daemon=True).start()


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Water Curtain ESP32 Config Tool")
    parser.add_argument("--esp",    default=ESP32_DEFAULT_IP, help="ESP32 IP")
    parser.add_argument("--ip",     help="Set remote IP")
    parser.add_argument("--port",   type=int, help="Set remote port")
    parser.add_argument("--info",   action="store_true", help="Get device info")
    parser.add_argument("--reset",  action="store_true", help="Reset ESP32")
    parser.add_argument("--scan",   nargs="?", const=get_local_network(), help="Scan network")
    parser.add_argument("--gui",    action="store_true", help="Open GUI")
    parser.add_argument("--cli",    action="store_true", help="Interactive CLI")
    
    args = parser.parse_args()
    
    if args.info:
        cmd_info(args.esp)
    elif args.ip:
        cmd_set_ip(args.esp, args.ip)
    elif args.port:
        cmd_set_port(args.esp, args.port)
    elif args.reset:
        cmd_reset(args.esp)
    elif args.scan is not None:
        print(f"[*] Scanning {args.scan}.* for devices...")
        devices = scan_for_devices(args.scan, callback=lambda ip: print(f"  [+] {ip}"))
        if devices:
            print(f"[OK] Found {len(devices)}: {', '.join(devices)}")
        else:
            print("[!] No devices found")
    elif args.cli:
        interactive_cli(args.esp)
    else:
        # Default: GUI
        root = tk.Tk()
        gui = ConfigGUI(root)
        root.mainloop()
