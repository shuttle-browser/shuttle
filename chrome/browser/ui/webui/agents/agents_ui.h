// Copyright 2026 The Agent Chrome Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_AGENTS_AGENTS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_AGENTS_AGENTS_UI_H_

#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/webui_config.h"

namespace agent_chrome {

inline constexpr char kChromeUIAgentsHost[] = "agents";

class AgentsUI;

class AgentsUIConfig : public content::DefaultWebUIConfig<AgentsUI> {
 public:
  AgentsUIConfig();
};

// chrome://agents: lists the agents with their sessions and tabs, and lets
// the user create, edit and delete agents, close sessions, open a tab as an
// agent and jump to an agent's tab. Uses the MCP tools for everything an
// agent can do too.
class AgentsUI : public content::WebUIController {
 public:
  explicit AgentsUI(content::WebUI* web_ui);
  AgentsUI(const AgentsUI&) = delete;
  AgentsUI& operator=(const AgentsUI&) = delete;
  ~AgentsUI() override;
};

}  // namespace agent_chrome

#endif  // CHROME_BROWSER_UI_WEBUI_AGENTS_AGENTS_UI_H_
