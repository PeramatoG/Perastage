#pragma once
#include <string>
#include <vector>

struct MvrXchangeNetworkInterface {
  std::string id;
  std::string displayName;
  std::string ipv4Address;
  bool isLoopback = false;
  bool isUp = false;
};

std::vector<MvrXchangeNetworkInterface> ListMvrXchangeNetworkInterfaces();
MvrXchangeNetworkInterface SelectMvrXchangeNetworkInterface(const std::string &selectedId);
std::string FormatMvrXchangeNetworkInterface(const MvrXchangeNetworkInterface &iface);
