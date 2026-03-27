import subprocess
import xml.etree.ElementTree as ET
import matplotlib.pyplot as plt
import re
import os

# Hàm chuyển đổi định dạng thời gian của NS-3 (vd: "+1000000000.0ns") sang giây (float)
def parse_ns3_time(time_str):
    # Regex gắp cả phần số (hỗ trợ định dạng e+...) và phần đơn vị (ns, ms, s...)
    match = re.search(r'([+-]?\d*\.?\d+(?:[eE][+-]?\d+)?)(ns|ms|us|fs|ps|s)?', time_str)
    if match:
        value = float(match.group(1))
        unit = match.group(2)
        
        # Chuyển đổi mọi thứ về chuẩn là Giây (Seconds)
        if unit == 's':
            return value
        elif unit == 'ms':
            return value * 1e-3
        elif unit == 'us':
            return value * 1e-6
        elif unit == 'ns':
            return value * 1e-9
        elif unit == 'ps':
            return value * 1e-12
        else:
            # Mặc định phòng hờ nếu chuỗi không có đuôi đơn vị
            return value * 1e-9 
    return 0.0

# Các mốc số lượng Node cần chạy mô phỏng
nodes_list = list(range(1, 31))

# Danh sách lưu trữ kết quả để vẽ biểu đồ
throughputs = []
pdrs = []
delays = []

sim_time = 20.0 # Thời gian mô phỏng (theo cấu hình C++)

print("Bắt đầu chạy tự động mô phỏng Wi-Fi Ad-hoc (CSMA/CA Không có RTS/CTS)...")

for n in nodes_list:
    print(f"[{n}/30 Nodes] Đang chạy mô phỏng...")
    
    # 1. Gọi lệnh chạy NS-3
    cmd = f'./ns3 run "scratch/wifi-adhoc-csmaca --nNodes={n}"'
    subprocess.run(cmd, shell=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    
    # 2. Đọc file XML xuất ra từ FlowMonitor
    tree = ET.parse('wifi-adhoc-results.xml')
    root = tree.getroot()
    
    tx_packets = 0
    rx_packets = 0
    rx_bytes = 0
    delay_sum = 0.0
  # Duyệt qua tất cả các luồng dữ liệu (Chỉ lấy trong phần FlowStats)
    for flow in root.findall('.//FlowStats/Flow'):
        tx_packets += int(flow.get('txPackets', 0))
        rx_packets += int(flow.get('rxPackets', 0))
        rx_bytes += int(flow.get('rxBytes', 0))
        delay_sum += parse_ns3_time(flow.get('delaySum', '+0.0ns'))
        
    # 3. Tính toán 3 chỉ số đánh giá (Metrics)
    throughput_kbps = (rx_bytes * 8) / (sim_time * 1000)
    pdr = (rx_packets / tx_packets * 100) if tx_packets > 0 else 0
    avg_delay_ms = (delay_sum / rx_packets * 1000) if rx_packets > 0 else 0
    
    throughputs.append(throughput_kbps)
    pdrs.append(pdr)
    delays.append(avg_delay_ms)
    
    print(f"   => Thông lượng: {throughput_kbps:.2f} Kbps | PDR: {pdr:.2f}% | Trễ: {avg_delay_ms:.2f} ms")

print("Đã chạy xong toàn bộ! Đang tiến hành vẽ đồ thị...")

# --- PHẦN VẼ BIỂU ĐỒ (MATPLOTLIB) ---
fig, (ax1, ax2, ax3) = plt.subplots(3, 1, figsize=(8, 12))
fig.suptitle('Đánh giá hiệu năng CSMA/CA (Không có RTS/CTS)\nMạng Wi-Fi Ad-hoc', fontsize=14, fontweight='bold')

# Biểu đồ 1: Thông lượng (Throughput)
ax1.plot(nodes_list, throughputs, marker='o', color='b', linestyle='-', linewidth=2)
ax1.set_ylabel('Thông lượng (Kbps)', fontweight='bold')
ax1.set_title('Tổng thông lượng mạng (Total Throughput)')
ax1.grid(True, linestyle='--', alpha=0.7)

# Biểu đồ 2: Tỷ lệ giao gói thành công (PDR)
ax2.plot(nodes_list, pdrs, marker='s', color='g', linestyle='-', linewidth=2)
ax2.set_ylabel('PDR (%)', fontweight='bold')
ax2.set_title('Tỷ lệ giao gói thành công (Packet Delivery Ratio)')
ax2.set_ylim(0, 105)
ax2.grid(True, linestyle='--', alpha=0.7)

# Biểu đồ 3: Độ trễ trung bình (End-to-End Delay)
ax3.plot(nodes_list, delays, marker='^', color='r', linestyle='-', linewidth=2)
ax3.set_xlabel('Số lượng Nodes', fontweight='bold')
ax3.set_ylabel('Độ trễ (ms)', fontweight='bold')
ax3.set_title('Độ trễ trung bình (Average E2E Delay)')
ax3.grid(True, linestyle='--', alpha=0.7)

plt.tight_layout(rect=[0, 0.03, 1, 0.95])
plt.savefig('wifi_performance.png', dpi=300)
print("Đã lưu biểu đồ thành công vào file 'wifi_performance.png'")
