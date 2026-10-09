// Copyright 2026 Silkweave
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHUTTLE_MCP_TOOLS_H_
#define CHROME_BROWSER_SHUTTLE_MCP_TOOLS_H_

#include <string>
#include <string_view>

#include "base/containers/span.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/values.h"

namespace shuttle {

class ShuttleService;

// Result of an MCP tool call (`CallToolResult`).
struct ToolResult {
  ToolResult();
  ToolResult(ToolResult&&);
  ToolResult& operator=(ToolResult&&);
  ~ToolResult();

  // Text block with `value` as JSON, plus `structuredContent` for dicts.
  static ToolResult Json(base::Value value);
  static ToolResult Error(std::string message);
  static ToolResult Image(std::string base64_data, std::string mime_type);

  base::DictValue ToValue() &&;

  base::ListValue content;
  std::optional<base::DictValue> structured_content;
  bool is_error = false;
};

using ToolCallback = base::OnceCallback<void(ToolResult)>;

struct ToolContext {
  raw_ptr<ShuttleService> service;
  // Host header of the MCP request ("127.0.0.1:9222"), for CDP URLs.
  std::string host;
};

using ToolFunction = void (*)(const ToolContext& context,
                              const base::DictValue& args,
                              ToolCallback callback);

struct Tool {
  std::string_view name;
  std::string_view description;
  // JSON Schema of the arguments.
  std::string_view input_schema;
  ToolFunction run;
};

base::span<const Tool> GetTools();

}  // namespace shuttle

#endif  // CHROME_BROWSER_SHUTTLE_MCP_TOOLS_H_
