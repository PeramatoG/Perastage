#pragma once
#include "mvr_xchange_remote_station.h"
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace mvr::xchange {

enum class DnsRecordType { Ptr, Srv, Txt, A, Aaaa };

struct DnsRecord {
  DnsRecordType type = DnsRecordType::Ptr;
  std::string owner;
  std::string target;
  std::string address;
  std::map<std::string, std::string> text;
  std::uint16_t port = 0;
  std::uint32_t ttlSeconds = 0;
  std::uint32_t interfaceIndex = 0;
  std::uint64_t lastSeenMonotonicMs = 0;
};

class MdnsRecordCache {
public:
  struct CachedRecord {
    DnsRecord record;
    std::uint64_t expiryMonotonicMs = 0;
  };
  void Apply(DnsRecord record);
  void Expire(std::uint64_t nowMonotonicMs);
  void Clear();
  std::vector<MvrXchangeRemoteStation> Resolve(const std::string &groupServiceName,
                                                std::uint64_t nowMonotonicMs) const;
  std::size_t Size() const;

private:
  std::vector<CachedRecord> records_;
};

}
