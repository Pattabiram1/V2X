/* v2x-sim-reliable-final.cc
 *
 * Reliable & Real-time V2X simulation for ns-3.38
 * - Pre-populates ARP cache (no ARP delay, Node 0 sends reliably)
 * - Avoids duplicate QueueDisc install
 * - PCAP, FlowMonitor, ASCII traces, queue traces
 *
 * Build:
 *   ./ns3 build scratch/v2x-sim-reliable-final.cc
 *
 * Run example:
 *   ./ns3 run scratch/v2x-sim-reliable-final.cc -- --nVehicles=2 --simTime=12
 */

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/animation-interface.h"
#include "ns3/trace-helper.h"
#include "ns3/arp-cache.h"
#include "ns3/ipv4-l3-protocol.h"

#include <fstream>
#include <vector>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("V2XSimReliableFinal");

// --- Callbacks for sockets
void ReceivePacket(Ptr<Socket> socket)
{
    Ptr<Packet> packet;
    Address from;
    while ((packet = socket->RecvFrom(from)))
    {
        InetSocketAddress addr = InetSocketAddress::ConvertFrom(from);
        std::cout << Simulator::Now().GetSeconds() << "s: RSU received "
                  << packet->GetSize() << " bytes from " << addr.GetIpv4()
                  << std::endl;
    }
}

void SendPacket(Ptr<Socket> socket, Ipv4Address dst, uint16_t port, uint32_t vehId)
{
    Ptr<Packet> packet = Create<Packet>(100); // payload
    socket->SendTo(packet, 0, InetSocketAddress(dst, port));
    std::cout << Simulator::Now().GetSeconds() << "s: Vehicle " << vehId
              << " sent packet to RSU " << dst << ":" << port << std::endl;
}

// --- Queue trace callbacks
void QueueEnqueueCallback(Ptr<const QueueDiscItem> item)
{
    std::cout << Simulator::Now().GetSeconds() << "s: Queue Enqueue (pkt size="
              << item->GetPacket()->GetSize() << ")\n";
}
void QueueDequeueCallback(Ptr<const QueueDiscItem> item)
{
    std::cout << Simulator::Now().GetSeconds() << "s: Queue Dequeue (pkt size="
              << item->GetPacket()->GetSize() << ")\n";
}
void QueueDropCallback(Ptr<const QueueDiscItem> item)
{
    std::cout << Simulator::Now().GetSeconds() << "s: Queue Drop (pkt size="
              << item->GetPacket()->GetSize() << ")\n";
}

int main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);

    // --- CLI defaults
    bool enableFlowMonitor = true;
    bool enablePcap = true;
    bool enableNetAnim = false;
    bool enableQueueTraces = true;
    std::string netAnimFile = "v2x-sim-netanim.xml";
    uint32_t nVehicles = 2;
    double simTime = 12.0;

    CommandLine cmd;
    cmd.AddValue("enableFlowMonitor", "Enable FlowMonitor", enableFlowMonitor);
    cmd.AddValue("enablePcap", "Enable PCAP capture", enablePcap);
    cmd.AddValue("enableNetAnim", "Enable NetAnim XML output", enableNetAnim);
    cmd.AddValue("enableQueueTraces", "Enable Queue traces", enableQueueTraces);
    cmd.AddValue("netAnimFile", "NetAnim filename", netAnimFile);
    cmd.AddValue("nVehicles", "Number of vehicle nodes", nVehicles);
    cmd.AddValue("simTime", "Simulation stop time (s)", simTime);
    cmd.Parse(argc, argv);

    std::cout << "V2XSimReliableFinal: nVehicles=" << nVehicles
              << " simTime=" << simTime
              << "\n";

    // --- Nodes
    NodeContainer vehicles;
    vehicles.Create(nVehicles);
    NodeContainer rsu;
    rsu.Create(1);

    NodeContainer allNodes;
    allNodes.Add(vehicles);
    allNodes.Add(rsu);

    // --- Mobility
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    Ptr<ListPositionAllocator> posAlloc = CreateObject<ListPositionAllocator>();
    for (uint32_t i = 0; i < nVehicles; ++i)
        posAlloc->Add(Vector(5.0 + 20.0 * i, 0.0, 0.0));
    posAlloc->Add(Vector(25.0, 50.0, 0.0)); // RSU
    mobility.SetPositionAllocator(posAlloc);
    mobility.Install(allNodes);

    // --- Wifi
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211a);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                 "DataMode", StringValue("OfdmRate6Mbps"),
                                 "ControlMode", StringValue("OfdmRate6Mbps"));

    WifiMacHelper mac;
    mac.SetType("ns3::AdhocWifiMac");
    NetDeviceContainer devices = wifi.Install(phy, mac, allNodes);

    // --- Internet
    InternetStackHelper internet;
    internet.Install(allNodes);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    // --- Pre-populate ARP cache
    Ptr<NetDevice> rsuDev = devices.Get(nVehicles); // RSU device
    Mac48Address rsuMac = Mac48Address::ConvertFrom(rsuDev->GetAddress());
    Ipv4Address rsuIp = interfaces.GetAddress(nVehicles);

    for (uint32_t i = 0; i < nVehicles; ++i)
    {
        Ptr<Node> vehNode = vehicles.Get(i);
        Ptr<Ipv4L3Protocol> ipv4proto = vehNode->GetObject<Ipv4L3Protocol>();
        Ptr<ArpCache> arp = CreateObject<ArpCache>();
        arp->SetAliveTimeout(Seconds(3600));
        ArpCache::Entry *entry = arp->Add(rsuIp);
        entry->SetMacAddress(rsuMac);
        entry->MarkPermanent();

        for (uint32_t j = 0; j < ipv4proto->GetNInterfaces(); j++)
        {
            Ptr<Ipv4Interface> iface = ipv4proto->GetInterface(j);
            iface->SetArpCache(arp);
        }
        std::cout << "Pre-populated ARP for Vehicle " << i
                  << " → RSU " << rsuIp << " (" << rsuMac << ")\n";
    }

    // --- TrafficControl (QueueDisc) installation, avoid double-install
    TrafficControlHelper tch;
    tch.SetRootQueueDisc("ns3::PfifoFastQueueDisc");

    NetDeviceContainer devicesToInstall;
    for (uint32_t i = 0; i < devices.GetN(); ++i)
    {
        Ptr<NetDevice> dev = devices.Get(i);
        Ptr<Node> node = dev->GetNode();
        Ptr<TrafficControlLayer> tc = node->GetObject<TrafficControlLayer>();
        bool alreadyHas = false;
        if (tc)
        {
            Ptr<QueueDisc> existing = tc->GetRootQueueDiscOnDevice(dev);
            if (existing)
            {
                alreadyHas = true;
            }
        }
        if (!alreadyHas)
        {
            devicesToInstall.Add(dev);
        }
        else
        {
            std::cout << "Skipping QueueDisc install on device " << i << " (already has root qdisc)\n";
        }
    }

    if (devicesToInstall.GetN() > 0)
    {
        tch.Install(devicesToInstall);
        std::cout << "TrafficControl: installed QueueDiscs on "
                  << devicesToInstall.GetN() << " devices\n";
    }
    else
    {
        std::cout << "TrafficControl: no devices required QueueDisc install\n";
    }

    // --- RSU socket
    uint16_t port = 5000;
    Ptr<Socket> rsuSocket = Socket::CreateSocket(rsu.Get(0), UdpSocketFactory::GetTypeId());
    rsuSocket->Bind(InetSocketAddress(Ipv4Address::GetAny(), port));
    rsuSocket->SetRecvCallback(MakeCallback(&ReceivePacket));

    // --- Vehicle sockets
    std::vector< Ptr<Socket> > vehicleSockets(nVehicles);
    for (uint32_t i = 0; i < nVehicles; ++i)
        vehicleSockets[i] = Socket::CreateSocket(vehicles.Get(i), UdpSocketFactory::GetTypeId());

    // --- Schedule sends
    double sendStart = 1.0;
    for (uint32_t i = 0; i < nVehicles; ++i)
    {
        Ipv4Address rsuAddr = rsuIp;
        uint32_t vehId = i + 1;
        double tsend = sendStart + double(i);
        Simulator::Schedule(Seconds(tsend),
                            [sock = vehicleSockets[i], rsuAddr, port, vehId]() {
                                SendPacket(sock, rsuAddr, port, vehId);
                            });
        Simulator::Schedule(Seconds(tsend + 1.0),
                            [sock = vehicleSockets[i], rsuAddr, port, vehId]() {
                                SendPacket(sock, rsuAddr, port, vehId);
                            });
    }

    // --- Tracing
    if (enablePcap) phy.EnablePcapAll("v2x-sim-final", true);

    AsciiTraceHelper ascii;
    Ptr<OutputStreamWrapper> asciiStream = ascii.CreateFileStream("v2x-sim-final.tr");
    phy.EnableAsciiAll(asciiStream);

    // --- FlowMonitor
    FlowMonitorHelper fmHelper;
    Ptr<FlowMonitor> flowMonitor = nullptr;
    if (enableFlowMonitor) flowMonitor = fmHelper.InstallAll();

    // --- Run
    Simulator::Stop(Seconds(simTime));
    Simulator::Run();

    if (enableFlowMonitor && flowMonitor)
    {
        flowMonitor->CheckForLostPackets();
        flowMonitor->SerializeToXmlFile("v2x-sim-final-flowmon.xml", true, true);
    }

    Simulator::Destroy();
    return 0;
}

