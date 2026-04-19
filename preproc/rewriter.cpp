#include "rewriter.hpp"

#include <set>
#include <string>

#include "util.hpp"

namespace preproc {

void assign_ids(Schema& schema, std::vector<FileAnalysis>& analyses) {
  // used_* tracks all IDs that are spoken for (schema + source + newly allocated).
  // claimed_events tracks IDs assigned to a call this run — detects duplicates.
  std::set<std::uint64_t> used_events;
  std::set<std::uint64_t> used_tags;
  std::set<std::uint64_t> claimed_events;

  // Seed from schema
  for (const auto& [name, id] : schema.tag_ids) {
    used_tags.insert(id);
  }
  for (const auto& [id, positions] : schema.event_positions) {
    used_events.insert(id);
  }

  // Reserve all source event IDs so alloc_event skips them.
  for (const auto& analysis : analyses) {
    for (const auto& call : analysis.calls) {
      if (!call.existing_ids.empty()) {
        used_events.insert(call.existing_ids[0]);
      }
    }
  }

  std::uint64_t next_event = 0;
  std::uint64_t next_tag = 0;

  auto alloc_event = [&]() {
    while (used_events.count(next_event) > 0) {
      ++next_event;
    }
    auto id = next_event++;
    used_events.insert(id);
    return id;
  };

  auto alloc_tag = [&]() {
    while (used_tags.count(next_tag) > 0) {
      ++next_tag;
    }
    auto id = next_tag++;
    used_tags.insert(id);
    return id;
  };

  // Clear event_positions — rebuild from source only (removes stale events).
  schema.event_positions.clear();

  for (auto& analysis : analyses) {
    for (auto& call : analysis.calls) {
      if (call.chain.empty()) continue;

      // --- resolve tag IDs from schema (by string) ---
      std::vector<std::uint64_t> tag_ids;
      tag_ids.reserve(call.chain.size());

      for (const auto& chain_call : call.chain) {
        if (!chain_call.tag.empty()) {
          auto it = schema.tag_ids.find(chain_call.tag);
          if (it != schema.tag_ids.end()) {
            tag_ids.push_back(it->second);
          } else {
            auto id = alloc_tag();
            schema.tag_ids[chain_call.tag] = id;
            schema.tag_names[id] = chain_call.tag;
            tag_ids.push_back(id);
          }
        } else {
          tag_ids.push_back(alloc_tag());
        }
      }

      // --- resolve event ID ---
      // Keep source event ID if it hasn't been claimed by another call yet.
      // If duplicate or missing, allocate a new one.
      std::uint64_t event_id;
      if (!call.existing_ids.empty() && claimed_events.count(call.existing_ids[0]) == 0) {
        event_id = call.existing_ids[0];
      } else {
        event_id = alloc_event();
      }
      claimed_events.insert(event_id);

      std::vector<std::uint64_t> new_ids;
      new_ids.reserve(1 + tag_ids.size());
      new_ids.push_back(event_id);
      for (auto tid : tag_ids) {
        new_ids.push_back(tid);
      }

      // --- update schema with event positions ---
      // Track the flat position in the decoded value stream.
      // Each ChainCall normally starts a new pair (+2 positions).
      // "cstr_val" is the second tag within the same pair (+1 position).
      std::vector<std::size_t> positions;
      positions.reserve(call.chain.size());
      std::size_t flat_pos = 2;  // skip pair 0 (event_id, level)
      for (std::size_t i = 0; i < call.chain.size(); ++i) {
        positions.push_back(flat_pos);
        if (call.chain[i].method == "cstr_val") {
          ++flat_pos;  // second tag in the same pair, advance to next pair
        } else if (i + 1 < call.chain.size() && call.chain[i + 1].method == "cstr_val") {
          ++flat_pos;  // first tag of cstr pair, next entry takes the second slot
        } else {
          flat_pos += 2;  // normal pair, skip to next
        }
      }
      schema.event_positions[event_id] = std::move(positions);

      call.existing_ids = std::move(new_ids);
    }
  }

  // Clean stale tags: remove tags from schema that no longer appear in any source.
  std::set<std::uint64_t> live_tags;
  for (const auto& analysis : analyses) {
    for (const auto& call : analysis.calls) {
      for (std::size_t i = 1; i < call.existing_ids.size(); ++i) {
        live_tags.insert(call.existing_ids[i]);
      }
    }
  }

  std::vector<std::string> dead_tag_names;
  for (const auto& [name, id] : schema.tag_ids) {
    if (live_tags.count(id) == 0) {
      dead_tag_names.push_back(name);
    }
  }
  for (const auto& name : dead_tag_names) {
    auto id = schema.tag_ids[name];
    schema.tag_ids.erase(name);
    schema.tag_names.erase(id);
  }
}

void rewrite_sources(std::vector<FileAnalysis>& analyses) {
  for (auto& analysis : analyses) {
    if (analysis.calls.empty()) {
      continue;
    }

    std::string updated;
    updated.reserve(analysis.content.size() + analysis.calls.size() * 32);
    std::size_t cursor = 0;
    bool changed = false;

    for (const auto& call : analysis.calls) {
      std::size_t needed = 1 + call.chain.size();
      if (call.existing_ids.size() != needed) {
        continue;
      }

      std::string id_list;
      for (std::size_t i = 0; i < call.existing_ids.size(); ++i) {
        if (i > 0) id_list += ", ";
        id_list += std::to_string(call.existing_ids[i]);
      }

      std::string_view old_content(analysis.content.data() + call.brace_open,
                                   call.brace_close - call.brace_open + 1);
      std::string new_content = "{" + id_list + "}";

      if (old_content == new_content) {
        continue;
      }

      changed = true;
      updated.append(analysis.content, cursor, call.brace_open - cursor);
      updated.append(new_content);
      cursor = call.brace_close + 1;
    }

    if (!changed) {
      continue;
    }

    updated.append(analysis.content, cursor, analysis.content.size() - cursor);
    analysis.content = std::move(updated);
    write_file(analysis.path, analysis.content);
  }
}

}  // namespace preproc
