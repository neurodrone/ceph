// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*- 
// vim: ts=8 sw=2 smarttab
/*
 * Ceph - scalable distributed file system
 *
 * Copyright (C) 2004-2006 Sage Weil <sage@newdream.net>
 *
 * This is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License version 2.1, as published by the Free Software 
 * Foundation.  See file COPYING.
 * 
 */

#ifndef CEPH_LOGENTRY_H
#define CEPH_LOGENTRY_H

#include "include/utime.h"
#include "msg/msg_types.h"
#include "common/entity_name.h"

namespace ceph {
  class Formatter;
}

typedef enum {
  CLOG_DEBUG = 0,
  CLOG_INFO = 1,
  CLOG_SEC = 2,
  CLOG_WARN = 3,
  CLOG_ERROR = 4,
  CLOG_UNKNOWN = -1,
} clog_type;

static const std::string CLOG_CHANNEL_NONE    = "none";
static const std::string CLOG_CHANNEL_DEFAULT = "cluster";
static const std::string CLOG_CHANNEL_CLUSTER = "cluster";
static const std::string CLOG_CHANNEL_AUDIT   = "audit";

// this is the key name used in the config options for the default, e.g.
//   default=true foo=false bar=false
static const std::string CLOG_CONFIG_DEFAULT_KEY = "default";

/*
 * Given a clog log_type, return the equivalent syslog priority
 */
int clog_type_to_syslog_level(clog_type t);

clog_type string_to_clog_type(const string& s);
int string_to_syslog_level(string s);
int string_to_syslog_facility(string s);

string clog_type_to_string(clog_type t);


struct LogEntryKey {
private:
  uint64_t _hash = 0;

  void _calc_hash() {
    hash<entity_name_t> h;
    _hash = seq + h(rank);
  }

  entity_name_t rank;
  utime_t stamp;
  uint64_t seq = 0;

public:
  LogEntryKey() {}
  LogEntryKey(const entity_name_t& w, utime_t t, uint64_t s)
    : rank(w), stamp(t), seq(s) {
    _calc_hash();
  }

  uint64_t get_hash() const {
    return _hash;
  }

  void dump(Formatter *f) const;
  static void generate_test_instances(list<LogEntryKey*>& o);

  friend bool operator==(const LogEntryKey& l, const LogEntryKey& r) {
    return l.rank == r.rank && l.stamp == r.stamp && l.seq == r.seq;
  }
};

namespace std {
  template<> struct hash<LogEntryKey> {
    size_t operator()(const LogEntryKey& r) const {
      return r.get_hash();
    }
  };
} // namespace std

struct LogEntry {
  EntityName name;
  entity_name_t rank;
  entity_addrvec_t addrs;
  utime_t stamp;
  uint64_t seq;
  clog_type prio;
  string msg;
  string channel;

  LogEntry() : seq(0), prio(CLOG_DEBUG) {}

  LogEntryKey key() const { return LogEntryKey(rank, stamp, seq); }

  void log_to_syslog(string level, string facility);

  void encode(bufferlist& bl, uint64_t features) const;
  void decode(bufferlist::const_iterator& bl);
  void dump(Formatter *f) const;
  static void generate_test_instances(list<LogEntry*>& o);
  static clog_type str_to_level(std::string const &str);
};
WRITE_CLASS_ENCODER_FEATURES(LogEntry)

struct LogSummary {
  version_t version;
  // channel -> [(seq#, entry), ...]
  map<string,list<pair<uint64_t,LogEntry>>> tail_by_channel;
  uint64_t seq = 0;
  ceph::unordered_set<LogEntryKey> keys;

  LogSummary() : version(0) {}

  void build_ordered_tail(list<LogEntry> *tail) const;

  void add(const LogEntry& e) {
    keys.insert(e.key());
    tail_by_channel[e.channel].push_back(make_pair(++seq, e));
  }
  void prune(size_t max) {
    for (auto& i : tail_by_channel) {
      while (i.second.size() > max) {
	keys.erase(i.second.front().second.key());
	i.second.pop_front();
      }
    }
  }
  bool contains(const LogEntryKey& k) const {
    return keys.count(k);
  }

  void encode(bufferlist& bl, uint64_t features) const;
  void decode(bufferlist::const_iterator& bl);
  void dump(Formatter *f) const;
  static void generate_test_instances(list<LogSummary*>& o);
};
WRITE_CLASS_ENCODER_FEATURES(LogSummary)

inline ostream& operator<<(ostream& out, const clog_type t)
{
  switch (t) {
  case CLOG_DEBUG:
    return out << "[DBG]";
  case CLOG_INFO:
    return out << "[INF]";
  case CLOG_SEC:
    return out << "[SEC]";
  case CLOG_WARN:
    return out << "[WRN]";
  case CLOG_ERROR:
    return out << "[ERR]";
  default:
    return out << "[???]";
  }
}

inline ostream& operator<<(ostream& out, const LogEntry& e)
{
  return out << e.stamp << " " << e.name << " (" << e.rank << ") "
	     << e.seq << " : "
             << e.channel << " " << e.prio << " " << e.msg;
}

#endif
