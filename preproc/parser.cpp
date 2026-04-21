#include "parser.hpp"

#include "util.hpp"

namespace preproc {

namespace {

std::vector<std::uint64_t> parse_id_list(std::string_view src, std::size_t start,
                                          std::size_t end) {
  std::vector<std::uint64_t> ids;
  std::size_t cur = start;
  while (cur < end) {
    skip_ws(src, cur);
    if (cur >= end) {
      break;
    }
    auto val = parse_uint_literal(src, cur);
    if (!val) {
      break;
    }
    ids.push_back(*val);
    skip_ws(src, cur);
    if (cur < end && src[cur] == ',') {
      ++cur;
    }
  }
  return ids;
}

// Returns 1 or 2 ChainCalls. Methods like cs() have two string args,
// each needing its own tag ID.
std::vector<ChainCall> parse_chain_calls(std::string_view src, std::size_t& cur) {
  skip_ws(src, cur);

  while (cur + 1 < src.size() && src[cur] == '/' && src[cur + 1] == '/') {
    while (cur < src.size() && src[cur] != '\n') {
      ++cur;
    }
    skip_ws(src, cur);
  }

  if (cur >= src.size() || src[cur] != '.') {
    return {};
  }
  ++cur;

  std::size_t name_start = cur;
  while (cur < src.size() &&
         (std::isalnum(static_cast<unsigned char>(src[cur])) != 0 || src[cur] == '_')) {
    ++cur;
  }
  std::string method(src.substr(name_start, cur - name_start));

  if (method == "write") {
    return {};
  }

  skip_ws(src, cur);
  if (cur >= src.size() || src[cur] != '(') {
    return {};
  }

  auto paren_close = find_matching(src, cur, '(', ')');
  if (!paren_close) {
    return {};
  }

  // Parse first string argument
  std::size_t arg_cur = cur + 1;
  auto first_tag = parse_string_literal(src, arg_cur);

  std::vector<ChainCall> result;
  ChainCall call;
  call.method = method;
  call.tag = first_tag.value_or("");
  result.push_back(std::move(call));

  // For cs: parse the second string argument as a separate tag
  if (method == "cs") {
    skip_ws(src, arg_cur);
    if (arg_cur < *paren_close && src[arg_cur] == ',') {
      ++arg_cur;
      auto second_tag = parse_string_literal(src, arg_cur);
      ChainCall val_call;
      val_call.method = "cs_val";
      val_call.tag = second_tag.value_or("");
      result.push_back(std::move(val_call));
    }
  }

  cur = *paren_close + 1;
  return result;
}

std::optional<LogCall> parse_log_call(std::string_view content, std::size_t call_start) {
  std::size_t cur = call_start;

  if (!consume_literal(content, cur, "bilog::log")) {
    return std::nullopt;
  }

  skip_ws(content, cur);
  if (cur >= content.size() || content[cur] != '(') {
    return std::nullopt;
  }

  auto paren_close = find_matching(content, cur, '(', ')');
  if (!paren_close) {
    return std::nullopt;
  }

  std::size_t inner = cur + 1;
  skip_ws(content, inner);
  if (inner >= content.size() || content[inner] != '{') {
    return std::nullopt;
  }

  auto brace_close = find_matching(content, inner, '{', '}');
  if (!brace_close) {
    return std::nullopt;
  }

  LogCall call;
  call.brace_open = inner;
  call.brace_close = *brace_close;
  call.line = line_number_at(content, call_start);
  call.existing_ids = parse_id_list(content, inner + 1, *brace_close);

  cur = *paren_close + 1;
  bool has_write = false;
  while (true) {
    // Peek ahead: is the next token .write()?
    std::size_t peek = cur;
    skip_ws(content, peek);
    while (peek + 1 < content.size() && content[peek] == '/' && content[peek + 1] == '/') {
      while (peek < content.size() && content[peek] != '\n') {
        ++peek;
      }
      skip_ws(content, peek);
    }
    if (peek < content.size() && content[peek] == '.') {
      auto method_start = peek + 1;
      auto method_end = method_start;
      while (method_end < content.size() &&
             (std::isalnum(static_cast<unsigned char>(content[method_end])) != 0 ||
              content[method_end] == '_')) {
        ++method_end;
      }
      if (content.substr(method_start, method_end - method_start) == "write") {
        has_write = true;
        break;
      }
    }

    auto calls = parse_chain_calls(content, cur);
    if (calls.empty()) {
      break;
    }
    for (auto& c : calls) {
      call.chain.push_back(std::move(c));
    }
  }

  if (call.chain.empty()) {
    return std::nullopt;
  }

  call.has_write = has_write;
  return call;
}

}  // namespace

FileAnalysis analyze_file(const std::filesystem::path& path) {
  FileAnalysis analysis;
  analysis.path = path;
  analysis.content = read_file(path);

  for (std::size_t pos = 0; pos < analysis.content.size(); ++pos) {
    if (analysis.content.compare(pos, 10, "bilog::log") != 0) {
      continue;
    }
    auto parsed = parse_log_call(analysis.content, pos);
    if (parsed) {
      if (!parsed->has_write) {
        analysis.errors.push_back(
            path.string() + ":" + std::to_string(parsed->line) +
            ": bilog::log() chain missing .write()");
      }
      analysis.calls.push_back(std::move(*parsed));
    }
  }

  return analysis;
}

}  // namespace preproc
