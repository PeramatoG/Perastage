#include "mvr_xchange_mdns_cache.h"
#include "mvr_xchange_dns_names.h"
#include "../../core/uuidutils.h"
#include <algorithm>
#include <limits>
#include <set>

namespace mvr::xchange {
namespace {

// Returns a stable identity for replacement of one cached DNS resource record.
std::string RecordIdentity(const DnsRecord &record) {
  std::string identity = std::to_string(static_cast<int>(record.type)) + "|" + NormalizeDnsName(record.owner);
  if (record.type == DnsRecordType::Ptr || record.type == DnsRecordType::Srv) identity += "|" + NormalizeDnsName(record.target);
  if (record.type == DnsRecordType::A || record.type == DnsRecordType::Aaaa) identity += "|" + record.address;
  return identity + "|" + std::to_string(record.interfaceIndex);
}

// Finds one non-expired record by type and normalized owner.
const DnsRecord *FindRecord(const std::vector<MdnsRecordCache::CachedRecord> &records,
                            DnsRecordType type, const std::string &owner,
                            std::uint64_t nowMonotonicMs) {
  const std::string normalizedOwner = NormalizeDnsName(owner);
  for (const auto &cached : records) {
    if (cached.expiryMonotonicMs > nowMonotonicMs && cached.record.type == type &&
        NormalizeDnsName(cached.record.owner) == normalizedOwner) return &cached.record;
  }
  return nullptr;
}

}

// Inserts, refreshes, or schedules RFC 6762 goodbye expiry for one record.
void MdnsRecordCache::Apply(DnsRecord record) {
  const std::string identity = RecordIdentity(record);
  auto existing = std::find_if(records_.begin(), records_.end(), [&](const auto &cached) {
    return RecordIdentity(cached.record) == identity;
  });
  if (record.ttlSeconds == 0 && existing == records_.end()) return;
  const std::uint64_t ttlMs = record.ttlSeconds == 0 ? 1000 : static_cast<std::uint64_t>(record.ttlSeconds) * 1000;
  const std::uint64_t expiry = record.lastSeenMonotonicMs > std::numeric_limits<std::uint64_t>::max() - ttlMs
                                   ? std::numeric_limits<std::uint64_t>::max()
                                   : record.lastSeenMonotonicMs + ttlMs;
  if (existing == records_.end()) records_.push_back({std::move(record), expiry});
  else {
    if (record.type == DnsRecordType::Txt && record.ttlSeconds != 0)
      record.text.insert(existing->record.text.begin(), existing->record.text.end());
    existing->record = std::move(record);
    existing->expiryMonotonicMs = expiry;
  }
}

// Applies one DNS message while replacing complete TXT RRsets atomically.
void MdnsRecordCache::ApplyBatch(std::vector<DnsRecord> records) {
  std::map<std::string, DnsRecord> mergedTxt;
  for (auto &record : records) {
    if (record.type != DnsRecordType::Txt) { Apply(std::move(record)); continue; }
    const std::string key = NormalizeDnsName(record.owner) + "|" + std::to_string(record.interfaceIndex);
    auto &merged = mergedTxt[key];
    if (merged.owner.empty()) merged = record;
    else {
      merged.text.insert(record.text.begin(), record.text.end());
      merged.ttlSeconds = std::min(merged.ttlSeconds, record.ttlSeconds);
      merged.lastSeenMonotonicMs = std::max(merged.lastSeenMonotonicMs, record.lastSeenMonotonicMs);
    }
  }
  for (auto &entry : mergedTxt) {
    auto &record = entry.second;
    const std::string identity = RecordIdentity(record);
    if (record.ttlSeconds != 0)
      records_.erase(std::remove_if(records_.begin(), records_.end(), [&](const auto &cached) { return RecordIdentity(cached.record) == identity; }), records_.end());
    Apply(std::move(record));
  }
}

// Removes records whose TTL or goodbye grace interval has elapsed.
void MdnsRecordCache::Expire(std::uint64_t nowMonotonicMs) {
  records_.erase(std::remove_if(records_.begin(), records_.end(), [&](const auto &cached) {
    return cached.expiryMonotonicMs <= nowMonotonicMs;
  }), records_.end());
}

// Clears all cached records during deterministic stop or interface restart.
void MdnsRecordCache::Clear() { records_.clear(); }

// Resolves cached records through official group and conventional DNS-SD layouts.
std::vector<MvrXchangeRemoteStation> MdnsRecordCache::Resolve(const std::string &groupServiceName,
                                                               std::uint64_t nowMonotonicMs) const {
  std::vector<MvrXchangeRemoteStation> stations;
  std::set<std::string> seenInstances;
  for (const auto &cached : records_) {
    const auto &ptr = cached.record;
    if (cached.expiryMonotonicMs <= nowMonotonicMs || ptr.type != DnsRecordType::Ptr) continue;
    const bool groupPtr = DnsNamesEqual(ptr.owner, groupServiceName);
    const std::string normalizedTarget = NormalizeDnsName(ptr.target);
    const std::string normalizedGroup = NormalizeDnsName(groupServiceName);
    const bool compatibleBasePtr = DnsNamesEqual(ptr.owner, kMvrXchangeServiceType) && normalizedTarget.size() > normalizedGroup.size() &&
                                   normalizedTarget[normalizedTarget.size() - normalizedGroup.size() - 1] == '.' &&
                                   normalizedTarget.compare(normalizedTarget.size() - normalizedGroup.size(), normalizedGroup.size(), normalizedGroup) == 0;
    if ((!groupPtr && !compatibleBasePtr) || !seenInstances.insert(NormalizeDnsName(ptr.target)).second) continue;
    const DnsRecord *srv = FindRecord(records_, DnsRecordType::Srv, ptr.target, nowMonotonicMs);
    const DnsRecord *txt = FindRecord(records_, DnsRecordType::Txt, ptr.target, nowMonotonicMs);
    if (!srv) srv = FindRecord(records_, DnsRecordType::Srv, groupServiceName, nowMonotonicMs);
    if (!txt) txt = FindRecord(records_, DnsRecordType::Txt, groupServiceName, nowMonotonicMs);
    if (!srv) continue;
    const DnsRecord *address = FindRecord(records_, DnsRecordType::A, srv->target, nowMonotonicMs);
    if (!address) address = FindRecord(records_, DnsRecordType::Aaaa, srv->target, nowMonotonicMs);
    if (!address) continue;
    MvrXchangeRemoteStation station;
    station.serviceInstanceName = ptr.target;
    station.normalizedDnsIdentity = NormalizeDnsName(ptr.target);
    station.hostName = srv->target;
    station.ipAddress = address->address;
    station.port = srv->port;
    station.discovered = true;
    station.ttlSeconds = std::min({ptr.ttlSeconds, srv->ttlSeconds, address->ttlSeconds});
    station.lastSeenMonotonicMs = std::min({ptr.lastSeenMonotonicMs, srv->lastSeenMonotonicMs, address->lastSeenMonotonicMs});
    if (txt) {
      station.ttlSeconds = std::min(station.ttlSeconds, txt->ttlSeconds);
      station.lastSeenMonotonicMs = std::min(station.lastSeenMonotonicMs, txt->lastSeenMonotonicMs);
      const auto name = txt->text.find("stationname");
      const auto uuid = txt->text.find("stationuuid");
      if (name != txt->text.end()) station.stationName = name->second;
      if (uuid != txt->text.end()) station.stationUuid = CanonicalizeUuid(uuid->second);
    }
    if (station.stationName.empty()) station.stationName = ptr.target.substr(0, ptr.target.find('.'));
    stations.push_back(std::move(station));
  }
  return stations;
}

// Returns the number of currently cached resource records.
std::size_t MdnsRecordCache::Size() const { return records_.size(); }

}
