// Construction of BCube Architecture
// Authors: Linh Vu, Daji Wong
// Further edits by Leonardo Pellegrina leonardo.pellegrina@studenti.unipd.it
// To be used with ns-3 3.21

/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2013 Nanyang Technological University 
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Authors: Linh Vu <linhvnl89@gmail.com>, Daji Wong <wong0204@e.ntu.edu.sg>
 */
#include <iostream>
#include <fstream>
#include <string>
#include <cassert>

#include "ns3/flow-monitor-module.h"
#include "ns3/bridge-helper.h"
#include "ns3/bridge-net-device.h"
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/csma-module.h"
#include "ns3/ipv4-nix-vector-helper.h"

/*
	- This work goes along with the paper "Towards Reproducible Performance Studies of Datacenter Network Architectures Using An Open-Source Simulation Approach"

	- The code is constructed in the following order:
		1. Creation of Node Containers 
		2. Initialize settings for On/Off Application
		3. Connect hosts level 0 switches
		4. Connect hosts level 1 switches
		5. Connect hosts level 2 switches
		6. Start Simulation

	- Addressing scheme:
		1. Address of host: 10.level.switch.0 /24
		2. Address of BCube switch: 10.level.switch.0 /16

	- On/Off Traffic of the simulation: addresses of client and server are randomly selected at every iteration	

	- Simulation Settings:
		- Number of nodes: 64-4096 
		- Number of BCube levels: 3 (k=2 is fixed)
		- Number of active connections: 0.1 * nodes - 100 * nodes
		- Number of iterations (with same settings): iterations
		- Simulation running time: 1 second
		- Packet size: 1024 bytes
		- Data rate for packet sending: 1 Mbps
		- Data rate for device channel: 1000 Mbps
		- Delay time for device: 0.001 ms
		- Communication pairs selection: Random Selection with uniform probability
		- Traffic flow pattern: Constant rate traffic
		- Routing protocol: Nix-Vector
        
        - Flowmonitor Statistics Output:
			- Formatted: output-bcube-leo-test-exp.txt
			- Raw: raw-bcube-leo-test-exp.txt
            
*/


using namespace ns3;
using namespace std;
// formatted output 
ofstream file1;
// raw data output
ofstream file2;

NS_LOG_COMPONENT_DEFINE ("BCube-Architecture");

// Function to create address string from numbers
//
char * toString(int a,int b, int c, int d){

	int first = a;
	int second = b;
	int third = c;
	int fourth = d;

	char *address =  new char[30];
	char firstOctet[30], secondOctet[30], thirdOctet[30], fourthOctet[30];	
	//address = firstOctet.secondOctet.thirdOctet.fourthOctet;

	bzero(address,30);

	snprintf(firstOctet,10,"%d",first);
	strcat(firstOctet,".");
	snprintf(secondOctet,10,"%d",second);
	strcat(secondOctet,".");
	snprintf(thirdOctet,10,"%d",third);
	strcat(thirdOctet,".");
	snprintf(fourthOctet,10,"%d",fourth);

	strcat(thirdOctet,fourthOctet);
	strcat(secondOctet,thirdOctet);
	strcat(firstOctet,secondOctet);
	strcat(address,firstOctet);

	return address;
}

// Main function
//
int main(int argc, char *argv[])
{

    double initial_ports =  4;
	double max_ports =  16;
	double delta_ports =  3;
	int ports = 0;
	double traffic = 1;
	int total_host = 0;
	
	file1.open ("output-bcube-leonpelleg.txt", std::ios_base::app);
	file1 << "BCube Simulation\n";
	file1.close();
	
	// increment ports 
	for(ports = initial_ports; ports < max_ports + 1 ; ports = ports + delta_ports) 
	{

//=========== Define parameters based on value of k ===========//
//
	int k = 2;			// number of BCube level, For BCube with 3 levels, level0 to level2, k should be set as 2			
	int n = ports;			// number of servers in one BCube;
	int num_sw = pow (n,k);		// number of switch at each level (all levels have same number of switch) = n^k;
	total_host = num_sw*n;	// total number of host
		//char filename [] = "statistics/BCube.xml";	// filename for Flow Monitor xml output file
		
		// increment the maximum number of active connections
		for(traffic = 0.1; traffic < 101; traffic = traffic * 10) 
		{
            
			// Number of apps relative to numer of hosts
			int total_apps = traffic * total_host; 
			
			// iterate the same simulation for stat reliability 
			int iterations = 10;
			
			// stats variables
			// avg results at each iteration
			double throughputs[iterations];
			double delays[iterations];
			double droprate[iterations];
			for(int t = 0; t < iterations; t++)
			{
				throughputs[t] = 0;
				delays[t] = 0;
				droprate[t] = 0;
			}
			
			// stat values of each flow
			double i_throughput = 0;
			double i_delay = 0;
			/// final stat values: min and max
			double min_throughput = 0;
			double max_throughput = 0;
			double min_delay = 0;
			double max_delay = 0;
			// final stat values: avg
			double m_throughput = 0;
			double m_delay = 0;
			double m_droprate = 0;
			// total number of flows: it can be different from total number of apps
			int total_flows = 0;
			
			// Start of the iterations
			for(int t = 0; t < iterations; t++){

// Initialize other variables
//
	int i = 0;		
	int j = 0;				
	int temp = 0;		

// Define variables for On/Off Application
// These values will be used to serve the purpose that addresses of server and client are selected randomly
// Note: the format of host's address is 10.pod.switch.(host+2)
//
	int levelRand = 0;	//	
	int swRand = 0;		// Random values for servers' address
	int hostRand = 0;	//

	int randHost =0;	// Random values for clients' address

// Initialize parameters for On/Off application
//
	int port = 9;
	int packetSize = 1024;		// 1024 bytes
	//char dataRate_OnOff [] = "1Mbps";
	char maxBytes [] = "0";		// unlimited

// Initialize parameters for Csma protocol
//
	char dataRate [] = "1000Mbps";	// 1Gbps
	int delay = 0.001;		// 0.001 ms


// Initialize Internet Stack and Routing Protocols
//	
	InternetStackHelper internet;
	Ipv4NixVectorHelper nixRouting; 
	Ipv4StaticRoutingHelper staticRouting;
	Ipv4ListRoutingHelper list;
	list.Add (staticRouting, 0);	
	list.Add (nixRouting, 10);	
	internet.SetRoutingHelper(list);	

//=========== Creation of Node Containers ===========//
//
	NodeContainer host;				// NodeContainer for hosts;  				
	host.Create (total_host);				
	internet.Install (host);			

	NodeContainer swB0;				// NodeContainer for B0 switches 
	swB0.Create (num_sw);				
	internet.Install (swB0);
				
	NodeContainer bridgeB0;				// NodeContainer for B0 bridges
  	bridgeB0.Create (num_sw);
	internet.Install (bridgeB0);

	NodeContainer swB1;				// NodeContainer for B1 switches
	swB1.Create (num_sw);				
	internet.Install (swB1);			

	NodeContainer bridgeB1;				// NodeContainer for B1 bridges
  	bridgeB1.Create (num_sw);
	internet.Install (bridgeB1);

	NodeContainer swB2;				// NodeContainer for B2 switches
	swB2.Create (num_sw);				
	internet.Install (swB2);			

	NodeContainer bridgeB2;				// NodeContainer for B2 bridges
	bridgeB2.Create (num_sw);
	internet.Install (bridgeB2);


//=========== Initialize settings for On/Off Application ===========//
//

// Generate traffics for the simulation
//
	// store the apps into dynamic memory
	std::vector<ApplicationContainer> app(total_apps);  
	// seed the rand() functions
	srand(time(0));
		
	for (i=0;i<total_apps;i++)
	{
	// Randomly select a server
		levelRand = 0;
		swRand = rand() % num_sw + 0;
		hostRand = rand() % n + 0;
		hostRand = hostRand+2;
		char *add;
		add = toString(10, levelRand, swRand, hostRand);

	// Initialize On/Off Application with addresss of server
		OnOffHelper oo = OnOffHelper("ns3::UdpSocketFactory",Address(InetSocketAddress(Ipv4Address(add), port))); // ip address of server
   
			// constant rate traffic
			oo.SetConstantRate(DataRate("1Mbps") , packetSize);
	        oo.SetAttribute("MaxBytes",StringValue (maxBytes));
			//oo.SetAttribute("OnTime",StringValue("ns3::ExponentialRandomVariable[Mean=0.01]")); 
			//oo.SetAttribute("OffTime",StringValue("ns3::ExponentialRandomVariable[Mean=0.01]")); 
			//oo.SetAttribute("PacketSize",UintegerValue (packetSize));
			//oo.SetAttribute("DataRate",StringValue (dataRate_OnOff));      
			//oo.SetAttribute("MaxBytes",StringValue (maxBytes));

	// Randomly select a client
		randHost = rand() % total_host + 0;		
		int temp = n*swRand + (hostRand-2);
		while (temp== randHost){
			randHost = rand() % total_host + 0;
		} 
		// to make sure that client and server are different

	// Install On/Off Application to the client
		NodeContainer onoff;
		onoff.Add(host.Get(randHost));
	     	app[i] = oo.Install (onoff);
	}

	//std::cout << "Finished creating On/Off traffic"<<"\n";	
// Inintialize Address Helper
//	
  	Ipv4AddressHelper address;

// Initialize Csma helper
//
  	CsmaHelper csma;
  	csma.SetChannelAttribute ("DataRate", StringValue (dataRate));
  	csma.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (delay)));

//=========== Connect BCube 0 switches to hosts ===========//
//	
	NetDeviceContainer hostSwDevices0[num_sw];		
	NetDeviceContainer bridgeDevices0[num_sw];		
	Ipv4InterfaceContainer ipContainer0[num_sw];

	temp = 0;
	for (i=0;i<num_sw;i++){
		NetDeviceContainer link1 = csma.Install(NodeContainer (swB0.Get(i), bridgeB0.Get(i)));
		hostSwDevices0[i].Add(link1.Get(0));			
		bridgeDevices0[i].Add(link1.Get(1));			
		temp = j;
		for (j=temp;j<temp+n; j++){
			NetDeviceContainer link2 = csma.Install(NodeContainer (host.Get(j), bridgeB0.Get(i)));
			hostSwDevices0[i].Add(link2.Get(0));		
			bridgeDevices0[i].Add(link2.Get(1));			 
		}	
		BridgeHelper bHelper0;
		bHelper0.Install (bridgeB0.Get(i), bridgeDevices0[i]);	
		//Assign address
		char *subnet;
		subnet = toString(10, 0, i, 0);
		address.SetBase (subnet, "255.255.255.0");
		ipContainer0[i] = address.Assign(hostSwDevices0[i]);	
	}
	//std::cout <<"Fininshed BCube 0 connection"<<"\n";

//=========== Connect BCube 1 switches to hosts ===========//
//
	NetDeviceContainer hostSwDevices1[num_sw];		
	NetDeviceContainer bridgeDevices1[num_sw];		
	Ipv4InterfaceContainer ipContainer1[num_sw];
	
	j = 0; temp = 0;

	for (i=0;i<num_sw;i++){
		NetDeviceContainer link1 = csma.Install(NodeContainer (swB1.Get(i), bridgeB1.Get(i)));
		hostSwDevices1[i].Add(link1.Get(0));			
		bridgeDevices1[i].Add(link1.Get(1));
	
		if (i==0){
			j = 0; 
			temp = j;
		}

		if (i%n !=0){
			j = temp + 1;
			temp = j;
		}

		if ((i%n == 0)&&(i!=0)){
			j = temp - n + 1;
			j = j + n*n;
			temp = j;
		}
		
		for (j=temp;j<temp+n*n; j=j+n){
			NetDeviceContainer link2 = csma.Install(NodeContainer (host.Get(j), bridgeB1.Get(i)));
			hostSwDevices1[i].Add(link2.Get(0));		
			bridgeDevices1[i].Add(link2.Get(1));			 
		}	
		BridgeHelper bHelper1;
		bHelper1.Install (bridgeB1.Get(i), bridgeDevices1[i]);
		//Assign address
		char *subnet;
		subnet = toString(10, 1, i, 0);
		address.SetBase (subnet, "255.255.255.0");
		ipContainer1[i] = address.Assign(hostSwDevices1[i]);		
	}
	//std::cout <<"Fininshed BCube 1 connection"<<"\n";

//=========== Connect BCube 2 switches to hosts ===========//
//
	NetDeviceContainer hostSwDevices2[num_sw];		
	NetDeviceContainer bridgeDevices2[num_sw];	
	Ipv4InterfaceContainer ipContainer2[num_sw];
	
	j = 0; temp = 0; 
	int temp2 =n*n;
	int temp3 = n*n*n;

	for (i=0;i<num_sw;i++){
		NetDeviceContainer link1 = csma.Install(NodeContainer (swB2.Get(i), bridgeB2.Get(i)));
		hostSwDevices2[i].Add(link1.Get(0));			
		bridgeDevices2[i].Add(link1.Get(1));

		if (i==0){
			j = 0; 
			temp = j;
		}

		if (i%temp2 !=0){
			j = temp + 1;
			temp = j;
		}

		if ((i%temp2 == 0)&&(i!=0)){
			j = temp - temp2 + 1;
			j = j + temp3;
			temp = j;
		}

		for (j=temp;j<temp+temp3; j=j+temp2){
			NetDeviceContainer link2 = csma.Install(NodeContainer (host.Get(j), bridgeB2.Get(i)));
			hostSwDevices2[i].Add(link2.Get(0));		
			bridgeDevices2[i].Add(link2.Get(1)); 
		}	
		BridgeHelper bHelper2;
		bHelper2.Install (bridgeB2.Get(i), bridgeDevices2[i]);	
		//Assign address
		char *subnet;
		subnet = toString(10, 2, i, 0);
		address.SetBase (subnet, "255.255.255.0");
		ipContainer2[i] = address.Assign(hostSwDevices2[i]);
		
	}
	//std::cout <<"Fininshed BCube 2 connection"<<"\n";
	//std::cout << "------------- "<<"\n";

//=========== Start the simulation ===========//
//

// output in console the running simulation
	std::cout <<"simulating "<<total_host<<" host - "<<total_apps<<" apps...\n" ;
	//std::cout << "Start Simulation.. "<<"\n";
	for (i=0;i<total_apps;i++){
		app[i].Start (Seconds (0.0));
  		app[i].Stop (Seconds (1.0));
	}
  	Ipv4GlobalRoutingHelper::PopulateRoutingTables ();
// Calculate Throughput using Flowmonitor
//
  	FlowMonitorHelper flowmon;
	Ptr<FlowMonitor> monitor = flowmon.InstallAll();
// Run simulation.
//
  	NS_LOG_INFO ("Run Simulation.");
  	Simulator::Stop (Seconds(2.0));
  	Simulator::Run ();

  	monitor->CheckForLostPackets ();
  	//monitor->SerializeToXmlFile(filename, true, false);

		
				// Statistics 
				Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
				std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats ();
				i = 0;
				for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator iter = stats.begin (); iter != stats.end (); ++iter)
				{
					//Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (iter->first);
					//i_throughput = iter->second.rxBytes * 8.0 / (iter->second.timeLastRxPacket.GetSeconds()-iter->second.timeFirstTxPacket.GetSeconds()); // for debug purposes
					
					// if the flow has rxPackets = 0, also delaySum will return 0 
					// so to avoid nan we fix the throughput and delay values
					if( ! iter->second.rxPackets )
					{
						i_throughput = 0;
						i_delay = 5;
					}
					else
					{
						i_throughput = (iter->second.rxPackets * 1024 * 8) / iter->second.delaySum.GetSeconds() ;
						i_delay = iter->second.delaySum.GetSeconds() / iter->second.rxPackets;
					}
					// Min and Max values
					if(i == 0 && t==0)
					{
						min_throughput = i_throughput;
						max_throughput = i_throughput;
						min_delay = i_delay;
						max_delay = i_delay;
					}
					else{
						if(i_throughput < min_throughput)
						min_throughput = i_throughput;
						else
						if(i_throughput > max_throughput)
						max_throughput = i_throughput;
						if(i_delay < min_delay)
						min_delay = i_delay;
						else
						if(i_delay > max_delay)
						max_delay = i_delay;
					}
					// count che flows
					i++;
					// store in the arrays the candidates for the avg 
					throughputs[t] = throughputs[t] + (i_throughput / 1024);
					delays[t] = delays[t] + i_delay;
					droprate[t] = droprate[t] + (iter->second.txPackets - iter->second.rxPackets) / iter->second.txPackets;
                    
					// debug console output 
					//NS_LOG_UNCOND("Flow ID: " << iter->first << " Src Addr " << t.sourceAddress << " Dst Addr " << t.destinationAddress);
					//NS_LOG_UNCOND("Tx Packets = " << iter->second.txPackets);
					//NS_LOG_UNCOND("Rx Packets = " << iter->second.rxPackets);
					//NS_LOG_UNCOND("Throughput: " <<  i_throughput / 1024 << " Kbps");
					//NS_LOG_UNCOND("Delay: " <<  i_delay << " s\n");
				}
				
				total_flows = i;
				//monitor->SerializeToXmlFile(filename, true, false);
                throughputs[t] = throughputs[t] / double(total_flows);

  	Simulator::Destroy ();
	
                file2.open ("rawdata-bcube-leonpelleg.txt", std::ios_base::app);
			    file2 << total_host << ";"<< traffic << ";"<< total_apps << ";"<< throughputs[t] / 1024<< ";\n";
			    file2.close();
				
			}
			
			// calculate avg from the avgs of every iteration
			for(int i = 0; i < iterations; i++)
			{
				m_delay =  m_delay + (delays[i] / double(iterations));
				m_throughput = m_throughput + (throughputs[i]/ double(iterations));
				m_droprate = m_droprate + (droprate[i]/ double(iterations));
			}
			
			// output to formatted file
			file1.open ("output-bcube-leonpelleg.txt", std::ios_base::app);
			file1 << "Total number of hosts =  "<< total_host<<"\n";
			file1 << "Total number of applications =  "<< total_apps <<"\n";
			file1 <<"Minimum Delay: " <<  min_delay << " s\n";
			file1 <<"Maximum Delay: " <<  max_delay << " s\n";
			file1 <<"Minimum Throughput: " <<  min_throughput / 1024 / 1024 << " Mbps\n";
			file1 <<"Maximum Throughput: " <<  max_throughput / 1024 / 1024 << " Mbps\n";
			file1 <<"Average Delay: " <<  m_delay << " s\n";
			file1 <<"Average Throughput: " <<  m_throughput / 1024 << " Mbps\n";
			file1 <<"Average Packetloss rate: " <<  m_droprate * 100 << " %\n";
			file1 << "\n";
			file1.close();
			
			std::cout <<"Done\n" ;
			
		}
	}
  	NS_LOG_INFO ("Done.");

	return 0;
}
