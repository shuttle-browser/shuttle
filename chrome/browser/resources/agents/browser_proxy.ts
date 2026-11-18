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
  persistSessionCookies: boolean;
  createdAt: string;
  // Throwaway agents: deleted after idleMinutes without activity.
  throwaway?: boolean;
  idleMinutes?: number;
  expiresAt?: string;
  sessions: number;
  tabs: number;
  // Last use: the latest session activity or its creation.
  lastActiveAt?: string;
  // Never removed by the bulk cleanup.
  keep?: boolean;
}

export interface IdleAgent {
  id: string;
  name: string;
  lastActiveAt: string;
}

// Resource usage per agent, measured over its renderer processes (memory in
// bytes, cpu in percent of one core). Agents without processes are missing;
// `sampled` is false until the first sample (memory; shortly after the first
// ask), `cpuSampled` until the second (CPU needs two, ~2 s later).
export interface UsageSnapshot {
  sampled: boolean;
  cpuSampled: boolean;
  agents: Record<string, {memory: number, cpu: number, processes: number}>;
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
  persistSessionCookies?: boolean;
  keep?: boolean;
}

// The page runs the same MCP tools agents use (list-agents, create-agent,
// ...), plus two user-only actions.
export interface BrowserProxy {
  listAgents(): Promise<{agents: Agent[]}>;
  listSessions(): Promise<{sessions: Session[]}>;
  createAgent(id: string, identity: AgentIdentity): Promise<Agent>;
  updateAgent(id: string, identity: AgentIdentity): Promise<Agent>;
  deleteAgent(id: string): Promise<void>;
  // Persistent agents with no session and no activity for `days` days, minus
  // agents marked keep. dryRun lists them without deleting.
  deleteIdleAgents(days: number, dryRun: boolean, ids?: string[]):
      Promise<{agents: IdleAgent[], deleted: number, skipped?: string[]}>;
  closeSession(sessionId: string): Promise<void>;
  openTab(agentId: string, url: string): Promise<void>;
  showTab(targetId: string): Promise<void>;
  // This browser's MCP endpoint ('' if unknown).
  getMcpUrl(): Promise<string>;
  getUsage(): Promise<UsageSnapshot>;
  // Opens the Task Manager on its Agents tab.
  openTaskManager(): void;
  // Favicons (data: URLs) of agent tabs, by target id; tabs without one are
  // missing.
  getFavicons(targetIds: string[]): Promise<Record<string, string>>;
  closeTab(targetId: string): Promise<void>;
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

  deleteIdleAgents(days: number, dryRun: boolean, ids?: string[]) {
    return this.callTool_<
        {agents: IdleAgent[], deleted: number, skipped?: string[]}>(
        'delete-idle-agents',
        ids ? {olderThanDays: days, dryRun, ids} : {olderThanDays: days, dryRun});
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

  getMcpUrl(): Promise<string> {
    return sendWithPromise('getMcpUrl');
  }

  getUsage(): Promise<UsageSnapshot> {
    return sendWithPromise('getUsage');
  }
  openTaskManager() {
    chrome.send('openTaskManager');
  }

  getFavicons(targetIds: string[]): Promise<Record<string, string>> {
    return sendWithPromise('getFavicons', targetIds);
  }

  closeTab(targetId: string) {
    return this.callTool_<void>('release-tab', {targetId});
  }


  static getInstance(): BrowserProxy {
    return instance || (instance = new BrowserProxyImpl());
  }

  static setInstance(obj: BrowserProxy) {
    instance = obj;
  }
}

let instance: BrowserProxy|null = null;
