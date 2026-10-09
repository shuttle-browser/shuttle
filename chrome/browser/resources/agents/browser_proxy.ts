// Copyright 2026 Silkweave
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {sendWithPromise} from 'chrome://resources/js/cr.js';

export const AGENT_COLORS = [
  'grey', 'blue', 'red', 'yellow', 'green', 'pink', 'purple', 'cyan', 'orange',
] as const;

export interface Agent {
  id: string;
  name: string;
  color: string;
  avatar?: string;
  createdAt: string;
  sessions: number;
  tabs: number;
}

export interface AgentTab {
  targetId: string;
  url: string;
  title: string;
}

export interface Session {
  sessionId: string;
  agent: string;
  openedAt: string;
  tabs: AgentTab[];
}

export interface AgentIdentity {
  name?: string;
  color?: string;
  avatar?: string;
}

// The page runs the same MCP tools agents use (list-agents, create-agent,
// ...), plus two user-only actions.
export interface BrowserProxy {
  listAgents(): Promise<{agents: Agent[]}>;
  listSessions(): Promise<{sessions: Session[]}>;
  createAgent(id: string, identity: AgentIdentity): Promise<Agent>;
  updateAgent(id: string, identity: AgentIdentity): Promise<Agent>;
  deleteAgent(id: string): Promise<void>;
  closeSession(sessionId: string): Promise<void>;
  openTab(agentId: string, url: string): Promise<void>;
  showTab(targetId: string): Promise<void>;
}

export class BrowserProxyImpl implements BrowserProxy {
  private callTool_<T>(name: string, args: object = {}): Promise<T> {
    return sendWithPromise('callTool', name, args);
  }

  listAgents() {
    return this.callTool_<{agents: Agent[]}>('list-agents', {avatarData: true});
  }

  listSessions() {
    return this.callTool_<{sessions: Session[]}>('list-sessions');
  }

  createAgent(id: string, identity: AgentIdentity) {
    return this.callTool_<Agent>(
        'create-agent', {id, ...identity, avatarData: true});
  }

  updateAgent(id: string, identity: AgentIdentity) {
    return this.callTool_<Agent>(
        'update-agent', {id, ...identity, avatarData: true});
  }

  deleteAgent(id: string) {
    return this.callTool_<void>('delete-agent', {id});
  }

  closeSession(sessionId: string) {
    return this.callTool_<void>('close-session', {sessionId});
  }

  openTab(agentId: string, url: string): Promise<void> {
    return sendWithPromise('openTab', agentId, url);
  }

  showTab(targetId: string): Promise<void> {
    return sendWithPromise('showTab', targetId);
  }

  static getInstance(): BrowserProxy {
    return instance || (instance = new BrowserProxyImpl());
  }

  static setInstance(obj: BrowserProxy) {
    instance = obj;
  }
}

let instance: BrowserProxy|null = null;
