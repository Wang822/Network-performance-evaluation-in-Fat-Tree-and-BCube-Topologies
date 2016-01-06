// Construction of Fat-tree Architecture
// Authors: Linh Vu, Daji Wong
// Credits to ARFATH AHAMED email: arfathsm@gmail.com for compatibility changes with ns3 3.21
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
#include "ns3/random-variable.h"
/*
	- This work goes along with the paper "Towards Reproducible Performance Studies of Datacenter Network Architectures Using An Open-Source Simulation Approach"

	- The code is constructed in the following order:
		1. Creation of Node Containers 
		2. Initialize settings for On/Off Application
		3. Connect hosts to edge switches
		4. Connect edge switches to aggregate switches
		5. Connect aggregate switches to core switches
		6. Start Simulation

	- Addressing scheme:
		1. Address of host: 10.pod.switch.0 /24
		2. Address of edge and aggregation switch: 10.pod.switch.0 /16
		3. Address of core switch: 10.(group number + k).switch.0 /8
		(Note: there are k/2 group of core switch)

	- On/Off Traffic of the simulation: addresses of client and server are randomly selected at every iteration
	
	- Simulation Settings:
				- Number of nodes: 128-4394
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
				- Formatted: output-fat-tree-leo-test-exp.txt
				- Raw: raw-fat-tree-leo-test-exp.txt
			

*/

using namespace ns3;
using namespace std;
NS_LOG_COMPONENT_DEFINE ("Fat-Tree-Architecture");
// formatted output 
ofstream file1;  
// raw data output
ofstream file2;

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
	double initial_ports =  6;
	double max_ports =  26;
	double delta_ports =  3;
	int ports = 0;
	double traffic = 1;
	int total_host = 0;
	
	file1.open ("output-fat-tree-leonpelleg.txt", std::ios_base::app);
	file1 << "Fat Tree Simulation\n";
	file1.close();
	
	// increment ports 
	for(ports = initial_ports; ports < max_ports + 1; ports = ports + delta_ports) 
	{
		//=========== Define parameters based on value of k ===========//
		//
		int k = ports;			// number of ports per switch
		int num_pod = k;		// number of pod
		int num_host = (k/2);		// number of hosts under a switch
		int num_edge = (k/2);		// number of edge switch in a pod
		int num_bridge = num_edge;	// number of bridge in a pod
		int num_agg = (k/2);		// number of aggregation switch in a pod
		int num_group = k/2;		// number of group of core switches
		int num_core = (k/2);		// number of core switch in a group
		total_host = k*k*k/4;	// number of hosts in the entire network	
		//char filename [] = "statistics/Fat-tree.xml";// filename for Flow Monitor xml output file

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
			// final stat values: min and max
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
			for(int t = 0; t < iterations; t++)
			{
				
				// Define variables for On/Off Application
				// These values will be used to serve the purpose that addresses of server and client are selected randomly
				// Note: the format of host's address is 10.pod.switch.(host+2)
				//
				int podRand = 0;	//	
				int swRand = 0;		// Random values for servers' address
				int hostRand = 0;	//

				int rand1 =0;		//
				int rand2 =0;		// Random values for clients' address	
				int rand3 =0;		//

				// Initialize other variables
				//
				int i = 0;	
				int j = 0;	
				int h = 0;


				// Initialize parameters for On/Off application
				//
				int port = 9;
				int packetSize = 1024;		// 1024 bytes
				//char dataRate_OnOff [] = "1Mbps";
				char maxBytes [] = "0";		// unlimited

				// Initialize parameters for Csma and PointToPoint protocol
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
				NodeContainer core[num_group];				// NodeContainer for core switches
				for (i=0; i<num_group;i++){  	
					core[i].Create (num_core);
					internet.Install (core[i]);		
				}
				NodeContainer agg[num_pod];				// NodeContainer for aggregation switches
				for (i=0; i<num_pod;i++){  	
					agg[i].Create (num_agg);
					internet.Install (agg[i]);
				}
				NodeContainer edge[num_pod];				// NodeContainer for edge switches
				for (i=0; i<num_pod;i++){  	
					edge[i].Create (num_bridge);
					internet.Install (edge[i]);
				}
				NodeContainer bridge[num_pod];				// NodeContainer for edge bridges
				for (i=0; i<num_pod;i++){  	
					bridge[i].Create (num_bridge);
					internet.Install (bridge[i]);
				}
				NodeContainer host[num_pod][num_bridge];		// NodeContainer for hosts
				for (i=0; i<k;i++){
					for (j=0;j<num_bridge;j++){  	
						host[i][j].Create (num_host);		
						internet.Install (host[i][j]);
					}
				}

				//=========== Initialize settings for On/Off Application ===========//
				//

				// Generate traffics for the simulation
				//	
				// store the apps into dynamic memory
				std::vector<ApplicationContainer> app(total_apps);  
				// seed the rand() functions
				srand(time(0));
				
				for (i=0;i<total_apps;i++){	
					// Randomly select a server
					podRand = rand() % num_pod + 0;
					swRand = rand() % num_edge + 0;
					hostRand = rand() % num_host + 0;
					hostRand = hostRand+2;
					char *add;
					add = toString(10, podRand, swRand, hostRand);

					// Initialize On/Off Application with addresss of server
					OnOffHelper oo = OnOffHelper("ns3::UdpSocketFactory",Address(InetSocketAddress(Ipv4Address(add), port))); // ip address of server

					/*		---------------*********CODE THAT IS CHANGED FOR VERSIONS HIGHER THAN NS-3.15************------------------
							The way random variable are used.
						CHANGED BY: ARFATH AHAMED email: arfathsm@gmail.com
		oo.SetAttribute("OnTime",RandomVariableValue(ExponentialVariable(1)));  
			oo.SetAttribute("OffTime",RandomVariableValue(ExponentialVariable(1)));
	*/


					// constant rate traffic
					oo.SetConstantRate(DataRate("1Mbps") , packetSize);
					oo.SetAttribute("MaxBytes",StringValue (maxBytes));
					//oo.SetAttribute("OnTime",StringValue("ns3::ExponentialRandomVariable[Mean=0.01]"));  
					//oo.SetAttribute("OffTime",StringValue("ns3::ExponentialRandomVariable[Mean=0.01]")); 
					//oo.SetAttribute("PacketSize",UintegerValue (packetSize));
					//oo.SetAttribute("DataRate",StringValue (dataRate_OnOff));      
					//oo.SetAttribute("MaxBytes",StringValue (maxBytes));

					// Randomly select a client
					rand1 = rand() % num_pod + 0;
					rand2 = rand() % num_edge + 0;
					rand3 = rand() % num_host + 0;

					while (rand1== podRand && swRand == rand2 && (rand3+2) == hostRand){
						rand1 = rand() % num_pod + 0;
						rand2 = rand() % num_edge + 0;
						rand3 = rand() % num_host + 0;
					} // to make sure that client and server are different

					// Install On/Off Application to the client
					NodeContainer onoff;
					onoff.Add(host[rand1][rand2].Get(rand3));
					app[i] = oo.Install (onoff);
				}
				//std::cout << "Finished creating On/Off traffic"<<"\n";

				// Inintialize Address Helper
				//	
				Ipv4AddressHelper address;

				// Initialize PointtoPoint helper
				//	
				PointToPointHelper p2p;
				p2p.SetDeviceAttribute ("DataRate", StringValue (dataRate));
				p2p.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (delay)));

				// Initialize Csma helper
				//
				CsmaHelper csma;
				csma.SetChannelAttribute ("DataRate", StringValue (dataRate));
				csma.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (delay)));

				//=========== Connect edge switches to hosts ===========//
				//	
				NetDeviceContainer hostSw[num_pod][num_bridge];		
				NetDeviceContainer bridgeDevices[num_pod][num_bridge];	
				Ipv4InterfaceContainer ipContainer[num_pod][num_bridge];

				for (i=0;i<num_pod;i++){
					for (j=0;j<num_bridge; j++){
						NetDeviceContainer link1 = csma.Install(NodeContainer (edge[i].Get(j), bridge[i].Get(j)));
						hostSw[i][j].Add(link1.Get(0));				
						bridgeDevices[i][j].Add(link1.Get(1));			

						for (h=0; h< num_host;h++){			
							NetDeviceContainer link2 = csma.Install (NodeContainer (host[i][j].Get(h), bridge[i].Get(j)));
							hostSw[i][j].Add(link2.Get(0));			
							bridgeDevices[i][j].Add(link2.Get(1));						
						}

						BridgeHelper bHelper;
						bHelper.Install (bridge[i].Get(j), bridgeDevices[i][j]);
						//Assign address
						char *subnet;
						subnet = toString(10, i, j, 0);
						address.SetBase (subnet, "255.255.255.0");
						ipContainer[i][j] = address.Assign(hostSw[i][j]);			
					}
				}
				//std::cout << "Finished connecting edge switches and hosts  "<< "\n";

				//=========== Connect aggregate switches to edge switches ===========//
				//
				NetDeviceContainer ae[num_pod][num_agg][num_edge]; 	
				Ipv4InterfaceContainer ipAeContainer[num_pod][num_agg][num_edge];
				for (i=0;i<num_pod;i++){
					for (j=0;j<num_agg;j++){
						for (h=0;h<num_edge;h++){
							ae[i][j][h] = p2p.Install(agg[i].Get(j), edge[i].Get(h));

							int second_octet = i;		
							int third_octet = j+(k/2);	
							int fourth_octet;
							if (h==0) fourth_octet = 1;
							else fourth_octet = h*2+1;
							//Assign subnet
							char *subnet;
							subnet = toString(10, second_octet, third_octet, 0);
							//Assign base
							char *base;
							base = toString(0, 0, 0, fourth_octet);
							address.SetBase (subnet, "255.255.255.0",base);
							ipAeContainer[i][j][h] = address.Assign(ae[i][j][h]);
						}			
					}		
				}
				//std::cout << "Finished connecting aggregation switches and edge switches  "<< "\n";

				//=========== Connect core switches to aggregate switches ===========//
				//
				NetDeviceContainer ca[num_group][num_core][num_pod]; 		
				Ipv4InterfaceContainer ipCaContainer[num_group][num_core][num_pod];
				int fourth_octet =1;
				
				for (i=0; i<num_group; i++){		
					for (j=0; j < num_core; j++){
						fourth_octet = 1;
						for (h=0; h < num_pod; h++){			
							ca[i][j][h] = p2p.Install(core[i].Get(j), agg[h].Get(i)); 	

							int second_octet = k+i;		
							int third_octet = j;
							//Assign subnet
							char *subnet;
							subnet = toString(10, second_octet, third_octet, 0);
							//Assign base
							char *base;
							base = toString(0, 0, 0, fourth_octet);
							address.SetBase (subnet, "255.255.255.0",base);
							ipCaContainer[i][j][h] = address.Assign(ca[i][j][h]);
							fourth_octet +=2;
						}
					}
				}
				//std::cout << "Finished connecting core switches and aggregation switches  "<< "\n";
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
				//-------------------------**********ARFATH AHAMED**********----------------------------------------
				/*
	Use of NetAnim 
	Animation code added BY: ARFATH AHAMED email: arfathsm@gmail.com
*/
				//AnimationInterface anim("fattree.xml");
				// NodeContainer for core switches
				/*
	for (i=0; i<num_group;i++){  	
		anim.SetConstantPosition(core[i].Get(0),an+0.5,0.0);
	anim.SetConstantPosition(core[i].Get(1),an+1.5,0.0);
	an=an+2.0;	
	if(i==num_group-1){an=0.0;}
	}


// NodeContainer for aggregation switches
	for (i=0; i<num_pod;i++){  	
		anim.SetConstantPosition(agg[i].Get(0),an+0.5,1.0);
	anim.SetConstantPosition(agg[i].Get(1),an+1.0,1.0);
	an=an+1.0;	
	if(i==num_pod-1){
		an=0.0;}	
	}

// NodeContainer for edge switches
	for (i=0; i<num_pod;i++){  	
	anim.SetConstantPosition(edge[i].Get(0),an+0.5,2.0);
	anim.SetConstantPosition(edge[i].Get(1),an+1.0,2.0);
	an=an+1.0;	
	if(i==num_pod-1){
		an=0.0;}
	}	
	

// NodeContainer for edge bridges
	for (i=0; i<num_pod;i++){  	
	anim.SetConstantPosition(bridge[i].Get(0),an+0.5,3.0);
	anim.SetConstantPosition(bridge[i].Get(1),an+1.0,3.0);
	an=an+1.0;	
	if(i==num_pod-1){
		an=0.0;}
	}	
	
// NodeContainer for hosts
	for (i=0; i<k;i++){
		for (j=0;j<num_bridge;j++){  
	anim.SetConstantPosition(host[i][j].Get(0),an+0.0,4.0);
	anim.SetConstantPosition(host[i][j].Get(1),an+0.3,4.0);	
	an=an+0.6;
		}
	}  	

*/
				//-------------------------**********ARFATH AHAMED**********----------------------------------------

				//NS_LOG_INFO ("Run Simulation.");
				Simulator::Stop (Seconds(2.0));
				Simulator::Run ();

				monitor->CheckForLostPackets ();
				//monitor->SerializeToXmlFile(filename, true, true);

				//std::cout << "Simulation finished "<<"\n";

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
				
				file2.open ("rawdata-fattree-leonpelleg.txt", std::ios_base::app);
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
			file1.open ("output-fat-tree-leonpelleg.txt", std::ios_base::app);
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




