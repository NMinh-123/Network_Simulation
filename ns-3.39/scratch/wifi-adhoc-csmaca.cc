#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/config-store-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/aodv-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WifiAdhocCsmaCa");

int main (int argc, char *argv[])
{
  uint32_t nNodes = 10; // Số lượng node mặc định, có thể thay đổi qua Command Line
  double simulationTime = 20.0; // Thời gian chạy mô phỏng (giây)

  // 1. Nhận tham số từ dòng lệnh (để dễ dàng lặp từ 2 đến 30 node)
  CommandLine cmd;
  cmd.AddValue ("nNodes", "Number of wifi nodes", nNodes);
  cmd.Parse (argc, argv);

  NS_LOG_UNCOND("Running simulation with " << nNodes << " nodes.");

  // 2. TẮT RTS/CTS (Yêu cầu cốt lõi của đề bài)
  // Đặt ngưỡng RTS/CTS lên 999999 bytes, nghĩa là các gói tin bình thường sẽ không bao giờ kích hoạt RTS/CTS
  Config::SetDefault ("ns3::WifiRemoteStationManager::RtsCtsThreshold", StringValue ("999999"));
  
  // Cố định tốc độ truyền (để đánh giá thuần túy CSMA/CA, tránh việc tự động giảm tốc)
  Config::SetDefault ("ns3::WifiRemoteStationManager::NonUnicastMode", StringValue ("DsssRate1Mbps"));

  // 3. Tạo các Node
  NodeContainer wifiNodes;
  wifiNodes.Create (nNodes);

  // 4. Cấu hình Kênh truyền và Tầng Vật lý (PHY)
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy;
  phy.SetChannel (channel.Create ());

  // 5. Cấu hình Tầng MAC (Ad-hoc)
  WifiHelper wifi;
  wifi.SetStandard (WIFI_STANDARD_80211b);
  wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                "DataMode", StringValue ("DsssRate1Mbps"),
                                "ControlMode", StringValue ("DsssRate1Mbps"));

  WifiMacHelper mac;
  mac.SetType ("ns3::AdhocWifiMac"); // Chế độ Ad-hoc

  NetDeviceContainer wifiDevices = wifi.Install (phy, mac, wifiNodes);

  // 6. Cấu hình Vị trí & Di chuyển (Tất cả đứng yên trong lưới 50x50m)
  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue (0.0),
                                 "MinY", DoubleValue (0.0),
                                 "DeltaX", DoubleValue (5.0),
                                 "DeltaY", DoubleValue (5.0),
                                 "GridWidth", UintegerValue (5),
                                 "LayoutType", StringValue ("RowFirst"));
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (wifiNodes);

  // 7. Cấu hình Định tuyến (AODV) và IP
  AodvHelper aodv;
  InternetStackHelper stack;
  stack.SetRoutingHelper (aodv);
  stack.Install (wifiNodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer wifiInterfaces;
  wifiInterfaces = address.Assign (wifiDevices);

  // 8. Tạo Lưu lượng mạng (UDP CBR Traffic)
  // Chia một nửa số node làm nguồn phát (Sender), nửa kia làm đích thu (Receiver)
  uint16_t port = 9;
  for (uint32_t i = 0; i < nNodes / 2; ++i)
    {
      uint32_t sinkNodeId = i + (nNodes / 2); // Node đích

      // Cài đặt Packet Sink (Ứng dụng nhận) trên node đích
      PacketSinkHelper sink ("ns3::UdpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), port));
      ApplicationContainer sinkApps = sink.Install (wifiNodes.Get (sinkNodeId));
      sinkApps.Start (Seconds (1.0));
      sinkApps.Stop (Seconds (simulationTime));

      // Cài đặt OnOff Application (Ứng dụng gửi) trên node nguồn
      OnOffHelper onoff ("ns3::UdpSocketFactory", InetSocketAddress (wifiInterfaces.GetAddress (sinkNodeId), port));
      onoff.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
      onoff.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
      onoff.SetAttribute ("DataRate", StringValue ("500kbps")); // Tốc độ gửi mỗi luồng
      onoff.SetAttribute ("PacketSize", UintegerValue (1024));

      ApplicationContainer sourceApps = onoff.Install (wifiNodes.Get (i));
      // Bắt đầu rải rác để tránh đụng độ nhân tạo ở đúng giây thứ 1.5
      sourceApps.Start (Seconds (1.5 + (i * 0.01))); 
      sourceApps.Stop (Seconds (simulationTime));
    }

  // 9. Cấu hình FlowMonitor để thu thập dữ liệu (Throughput, Delay, PDR)
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

  // 10. Chạy mô phỏng
  Simulator::Stop (Seconds (simulationTime + 1.0));
  Simulator::Run ();

  // 11. Xuất dữ liệu FlowMonitor ra file XML (dùng cho bước Collection & Report)
  monitor->SerializeToXmlFile ("wifi-adhoc-results.xml", true, true);

  Simulator::Destroy ();
  return 0;
}
