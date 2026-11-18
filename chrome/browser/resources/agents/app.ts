// Copyright 2026 Silkweave
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/strings.m.js';
import 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.js';
import 'chrome://resources/cr_elements/cr_toggle/cr_toggle.js';
import 'chrome://resources/cr_elements/cr_toolbar/cr_toolbar.js';
import 'chrome://resources/cr_elements/icons.html.js';

import type {CrActionMenuElement} from 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import type {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './app.css.js';
import {getHtml} from './app.html.js';
import type {Agent, AgentIdentity, BrowserProxy, IdleAgent, Session, UsageSnapshot} from './browser_proxy.js';
import {AGENT_COLORS, BrowserProxyImpl} from './browser_proxy.js';

// Avatar images from files: edge length in px, and the browser's size limit.
const AVATAR_SIZE = 256;
const AVATAR_MAX_LENGTH = 256 * 1024;

// How often the page refreshes while visible (agents change things too).
const REFRESH_MS = 2000;

// Quick start: the user's open/closed choice (otherwise open while there are
// no agents).
const SORT_KEY = 'agentSort';
const DEFAULT_MCP_URL = 'http://127.0.0.1:9222/mcp';

export const EXAMPLE_PROMPT = 'Use the Shuttle browser (MCP server "shuttle"): ' +
    'create an agent named "Scout", open a session, go to ' +
    'news.ycombinator.com and list the top 5 stories with their links. ' +
    'Close the session when you\'re done.';

// Agent id rule of the MCP server (kebab-case, max 64).
const ID_PATTERN = /^[a-z0-9]+(-[a-z0-9]+)*$/;

export interface EditState {
  // Agent id; '' while creating.
  id: string;
  newId: string;
  name: string;
  color: string;
  avatar: string;
  persistSessionCookies: boolean;
  // Throwaway agents have no avatar and keep nothing on disk.
  throwaway: boolean;
  keep: boolean;
}

export interface AgentsAppElement {
  $: {
    deleteDialog: CrDialogElement,
    editDialog: CrDialogElement,
    menu: CrActionMenuElement,
    toolbarMenu: CrActionMenuElement,
    openDialog: CrDialogElement,
    cleanupDialog: CrDialogElement,
    helpDialog: CrDialogElement,
  };
}

export class AgentsAppElement extends CrLitElement {
  static get is() {
    return 'agents-app';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      agents_: {type: Array},
      sessions_: {type: Array},
      loaded_: {type: Boolean},
      error_: {type: String},
      search_: {type: String},
      edit_: {type: Object},
      editError_: {type: String},
      openAgent_: {type: Object},
      openUrl_: {type: String},
      openError_: {type: String},
      deleteAgent_: {type: Object},
      sort_: {type: String},
      usage_: {type: Object},
      cleanupDays_: {type: Number},
      cleanupAgents_: {type: Array},
      cleanupError_: {type: String},
      cleanupDaysValid_: {type: Boolean},
      mcpUrl_: {type: String},
      expanded_: {type: Object},
      menuAgent_: {type: Object},
      favicons_: {type: Object},
      copied_: {type: String},
    };
  }

  protected accessor agents_: Agent[] = [];
  protected accessor sessions_: Session[] = [];
  protected accessor loaded_: boolean = false;
  protected accessor error_: string = '';
  protected accessor search_: string = '';
  protected accessor edit_: EditState|null = null;
  protected accessor editError_: string = '';
  protected accessor openAgent_: Agent|null = null;
  protected accessor openUrl_: string = '';
  protected accessor openError_: string = '';
  protected accessor deleteAgent_: Agent|null = null;
  // 'name', 'recent' (most recently active first), 'memory' or 'cpu' (most
  // first).
  protected accessor sort_: string =
      window.localStorage.getItem(SORT_KEY) || 'name';
  protected accessor usage_: UsageSnapshot|null = null;
  protected accessor cleanupDays_: number = 14;
  protected accessor cleanupAgents_: IdleAgent[]|null = null;
  protected accessor cleanupError_: string = '';
  protected accessor cleanupDaysValid_: boolean = true;
  // Previews answer out of order when the days change quickly: only the
  // latest request may fill the list.
  private cleanupRequest_: number = 0;
  protected accessor mcpUrl_: string = DEFAULT_MCP_URL;
  // 'open', 'closed' or '' (automatic).
  // Agents whose sessions and tabs are shown (row expanded).
  protected accessor expanded_: Set<string> = new Set();
  // Which copy button just copied (shows "Copied").
  protected accessor copied_: string = '';

  protected readonly colors = AGENT_COLORS;
  private browserProxy_: BrowserProxy = BrowserProxyImpl.getInstance();
  private timer_: number|null = null;
  // Agent whose "more actions" menu is open.
  protected accessor menuAgent_: Agent|null = null;
  // Favicons of the tabs in expanded rows, by target id.
  protected accessor favicons_: Record<string, string> = {};
  private onVisibilityChange_ = () => this.schedule_();

  override connectedCallback() {
    super.connectedCallback();
    document.addEventListener('visibilitychange', this.onVisibilityChange_);
    this.refresh_();
    this.browserProxy_.getMcpUrl().then(url => {
      if (url) {
        this.mcpUrl_ = url;
      }
    });
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    document.removeEventListener('visibilitychange', this.onVisibilityChange_);
    this.stop_();
  }

  private stop_() {
    if (this.timer_ !== null) {
      clearTimeout(this.timer_);
      this.timer_ = null;
    }
  }

  private schedule_() {
    this.stop_();
    if (document.visibilityState === 'visible') {
      this.timer_ = setTimeout(() => this.refresh_(), REFRESH_MS);
    }
  }

  private async refresh_() {
    this.stop_();
    try {
      const [agents, sessions] = await Promise.all([
        this.browserProxy_.listAgents(),
        this.browserProxy_.listSessions(),
      ]);
      this.agents_ = agents.agents;
      this.sessions_ = sessions.sessions;
      this.error_ = '';
    } catch (e) {
      this.error_ = String(e);
    }
    this.loaded_ = true;
    await this.refreshFavicons_();
    // Usage is a nicety: a failure doesn't show as an error. Asking keeps the
    // browser sampling; it stops ~10 s after the page stops asking (hidden).
    try {
      this.usage_ = await this.browserProxy_.getUsage();
    } catch {
      this.usage_ = null;
    }
    this.schedule_();
  }

  // Runs a user action; shows its error and refreshes.
  private async act_(action: Promise<unknown>) {
    try {
      await action;
      this.error_ = '';
    } catch (e) {
      this.error_ = String(e);
    }
    await this.refresh_();
  }

  protected onHelpClick_() {
    this.$.helpDialog.showModal();
  }

  protected onHelpCloseClick_() {
    this.$.helpDialog.close();
  }

  // A click on the backdrop (outside the dialog box) closes it.
  protected onHelpDialogClick_(e: MouseEvent) {
    const box = this.$.helpDialog.getNative().getBoundingClientRect();
    if (e.clientX < box.left || e.clientX > box.right ||
        e.clientY < box.top || e.clientY > box.bottom) {
      this.$.helpDialog.close();
    }
  }

  protected isExpanded_(agent: Agent): boolean {
    return agent.sessions > 0 && this.expanded_.has(agent.id);
  }

  // The agent's sessions while its row is expanded, else none.
  protected shownSessionsOf_(agent: Agent): Session[] {
    return this.isExpanded_(agent) ? this.sessionsOf_(agent) : [];
  }

  private async refreshFavicons_() {
    const ids = this.agents_.flatMap(agent => this.shownSessionsOf_(agent))
                    .flatMap(session => session.tabs.map(tab => tab.targetId));
    if (!ids.length) {
      return;
    }
    try {
      this.favicons_ = await this.browserProxy_.getFavicons(ids);
    } catch {
      // Favicons are a nicety.
    }
  }

  protected onAgentToggleClick_(e: Event) {
    const agent = this.agentFor_(e);
    if (!agent || !agent.sessions) {
      return;
    }
    const expanded = new Set(this.expanded_);
    if (!expanded.delete(agent.id)) {
      expanded.add(agent.id);
    }
    this.expanded_ = expanded;
    this.refreshFavicons_();
  }

  // "5 agents, 2 working · 1.2 GB · 14% CPU" (usage once sampled).
  protected summaryText_(): string {
    const working = this.agents_.filter(a => a.sessions > 0).length;
    let text = `${this.plural_(this.agents_.length, 'agent')}, ${
        working} working`;
    if (this.usage_?.sampled && Object.keys(this.usage_.agents).length) {
      const all = Object.values(this.usage_.agents);
      const memory = all.reduce((sum, u) => sum + u.memory, 0);
      const cpu = all.reduce((sum, u) => sum + u.cpu, 0);
      if (memory) {
        text += ` · ${this.bytesText_(memory)}${this.cpuSuffix_(cpu)}`;
      }
    }
    return text;
  }

  // Second line of an agent row: id, sessions and tabs or last activity,
  // usage.
  protected agentDetailText_(agent: Agent): string {
    const parts = [agent.id];
    if (agent.sessions > 0) {
      // Sessions only matter when there are several (one per MCP client).
      if (agent.sessions > 1) {
        parts.push(this.plural_(agent.sessions, 'session'));
      }
      parts.push(this.plural_(agent.tabs, 'tab'));
    } else if (!agent.throwaway && agent.lastActiveAt) {
      parts.push(this.lastActiveText_(agent));
    }
    const usage = this.agentUsageText_(agent);
    if (usage) {
      parts.push(usage);
    }
    return parts.join(' · ');
  }

  protected examplePrompt_(): string {
    return EXAMPLE_PROMPT;
  }

  protected claudeCommand_(): string {
    return `claude mcp add --transport http shuttle ${this.mcpUrl_}`;
  }

  protected codexCommand_(): string {
    return `codex mcp add shuttle --url ${this.mcpUrl_}`;
  }

  protected async onCopyClick_(e: Event) {
    const button = e.currentTarget as HTMLElement;
    const key = button.dataset['copy']!;
    const text = ({
      url: this.mcpUrl_,
      claude: this.claudeCommand_(),
      codex: this.codexCommand_(),
      prompt: EXAMPLE_PROMPT,
    } as Record<string, string>)[key]!;
    try {
      await navigator.clipboard.writeText(text);
      this.copied_ = key;
      setTimeout(() => {
        if (this.copied_ === key) {
          this.copied_ = '';
        }
      }, 1500);
    } catch (err) {
      this.error_ = `Copy failed: ${err}`;
    }
  }

  protected visibleAgents_(): Agent[] {
    const query = this.search_.trim().toLowerCase();
    const agents = !query ? this.agents_ : this.agents_.filter(
        a => a.name.toLowerCase().includes(query) || a.id.includes(query));
    const sorted = [...agents];
    if (this.sort_ === 'recent') {
      const time = (a: Agent) =>
          a.lastActiveAt ? new Date(a.lastActiveAt).getTime() : 0;
      sorted.sort((a, b) => time(b) - time(a));
    } else if (this.sort_ === 'memory' || this.sort_ === 'cpu') {
      // Until the first sample is in, by name.
      const value = (a: Agent) => this.sort_ === 'memory' ?
          this.usageOf_(a)?.memory ?? 0 :
          this.usageOf_(a)?.cpu ?? 0;
      sorted.sort(
          (a, b) => value(b) - value(a) || a.name.localeCompare(b.name));
    } else {
      sorted.sort((a, b) => a.name.localeCompare(b.name));
    }
    return sorted;
  }

  protected onSortChange_(e: Event) {
    this.sort_ = (e.target as HTMLSelectElement).value;
    try {
      window.localStorage.setItem(SORT_KEY, this.sort_);
    } catch {
      // Not remembered.
    }
  }

  private usageOf_(agent: Agent) {
    return this.usage_?.sampled ? this.usage_.agents[agent.id] : undefined;
  }

  // "412 MB · 3.1% CPU" for agents with processes once sampled, else ''.
  protected agentUsageText_(agent: Agent): string {
    const usage = this.usageOf_(agent);
    // The first sample can come before the process footprint is measured.
    if (!usage || !usage.memory) {
      return '';
    }
    return `${this.bytesText_(usage.memory)}${this.cpuSuffix_(usage.cpu)}`;
  }

  // " · 3.1% CPU" once CPU is measured, else ''.
  private cpuSuffix_(cpu: number): string {
    if (!this.usage_?.cpuSampled) {
      return '';
    }
    return ` · ${cpu < 10 ? cpu.toFixed(1) : Math.round(cpu)}% CPU`;
  }

  protected bytesText_(bytes: number): string {
    const mb = bytes / (1024 * 1024);
    if (mb >= 1024) {
      return `${(mb / 1024).toFixed(1)} GB`;
    }
    return `${Math.round(mb)} MB`;
  }

  protected onToolbarMoreClick_(e: Event) {
    this.$.toolbarMenu.showAt(e.currentTarget as HTMLElement);
  }

  protected onTaskManagerClick_() {
    this.$.toolbarMenu.close();
    this.browserProxy_.openTaskManager();
  }

  protected lastActiveText_(agent: Agent): string {
    if (agent.sessions > 0) {
      return 'Active now';
    }
    return agent.lastActiveAt ? `Last active ${this.since_(agent.lastActiveAt)}` :
                                '';
  }

  protected sessionsOf_(agent: Agent): Session[] {
    return this.sessions_.filter(s => s.agent === agent.id);
  }

  protected initial_(agent: Agent): string {
    return (agent.name || agent.id).trim().charAt(0).toUpperCase();
  }

  protected since_(iso: string): string {
    const time = new Date(iso);
    const minutes = Math.round((Date.now() - time.getTime()) / 60000);
    if (minutes < 1) {
      return 'just now';
    }
    if (minutes < 60) {
      return `${minutes} min ago`;
    }
    if (minutes < 24 * 60) {
      return `${Math.round(minutes / 60)} h ago`;
    }
    return time.toLocaleDateString();
  }

  protected tabLabel_(title: string, url: string): string {
    return title || url || 'about:blank';
  }

  protected host_(url: string): string {
    try {
      return new URL(url).host || url;
    } catch {
      return url;
    }
  }

  protected plural_(n: number, word: string): string {
    return `${n} ${word}${n === 1 ? '' : 's'}`;
  }

  private agentFor_(e: Event): Agent|null {
    const id = (e.currentTarget as HTMLElement).dataset['agent'];
    return this.agents_.find(a => a.id === id) || null;
  }

  protected onSearchChanged_(e: CustomEvent<string>) {
    this.search_ = e.detail;
  }

  // Create / edit ----------------------------------------------------------

  protected onCreateClick_() {
    const used = new Set(this.agents_.map(a => a.color));
    const color = AGENT_COLORS.find(c => c !== 'grey' && !used.has(c)) ||
        AGENT_COLORS[1];
    this.showEditDialog_({
      id: '',
      newId: '',
      name: '',
      color,
      avatar: '',
      persistSessionCookies: true,
      throwaway: false,
      keep: false,
    });
  }

  protected onMoreClick_(e: Event) {
    this.menuAgent_ = this.agentFor_(e);
    this.$.menu.showAt(e.currentTarget as HTMLElement);
  }

  protected onMenuEditClick_() {
    this.$.menu.close();
    const agent = this.menuAgent_;
    if (agent) {
      this.showEditDialog_({
        id: agent.id,
        newId: agent.id,
        name: agent.name,
        color: agent.color,
        avatar: agent.avatar || '',
        persistSessionCookies: agent.persistSessionCookies !== false,
        throwaway: !!agent.throwaway,
        keep: !!agent.keep,
      });
    }
  }

  protected expiryText_(agent: Agent): string {
    const minutes = agent.idleMinutes ?? 15;
    let text = `Temporary agent: deleted with its tabs after ${minutes} min ` +
        'without activity';
    if (agent.expiresAt) {
      const time = new Date(agent.expiresAt).toLocaleTimeString(
          undefined, {hour: '2-digit', minute: '2-digit'});
      text += ` (about ${time})`;
    }
    return text + '. Its storage is kept only in memory.';
  }

  private async showEditDialog_(edit: EditState) {
    this.edit_ = edit;
    this.editError_ = '';
    await this.updateComplete;
    this.$.editDialog.showModal();
  }

  protected onNameValueChanged_(e: CustomEvent<{value: string}>) {
    if (!this.edit_) {
      return;
    }
    const name = e.detail.value;
    const edit = {...this.edit_, name};
    // The id follows the name while creating, until edited by hand.
    if (!this.edit_.id && this.edit_.newId === this.slug_(this.edit_.name)) {
      edit.newId = this.slug_(name);
    }
    this.edit_ = edit;
  }

  protected onIdValueChanged_(e: CustomEvent<{value: string}>) {
    if (this.edit_) {
      this.edit_ = {...this.edit_, newId: e.detail.value};
      this.editError_ = '';
    }
  }

  protected onAvatarValueChanged_(e: CustomEvent<{value: string}>) {
    // The URL field shows "" for an image from a file: that "" isn't an edit.
    if (this.edit_ && (e.detail.value || !this.isDataAvatar_())) {
      this.edit_ = {...this.edit_, avatar: e.detail.value};
    }
  }

  protected isDataAvatar_(): boolean {
    return !!this.edit_?.avatar.startsWith('data:');
  }

  protected onAvatarChooseClick_() {
    const input =
        this.shadowRoot.querySelector<HTMLInputElement>('#avatarFile');
    if (input) {
      input.value = '';
      input.click();
    }
  }

  protected onAvatarRemoveClick_() {
    if (this.edit_) {
      this.edit_ = {...this.edit_, avatar: ''};
    }
  }

  // Scales the chosen image down to a 256x256 square (center crop) and keeps
  // it as a data: URL, well under the browser's 256 KB avatar limit.
  protected async onAvatarFileChange_(e: Event) {
    const file = (e.target as HTMLInputElement).files?.[0];
    if (!file || !this.edit_) {
      return;
    }
    try {
      const bitmap = await createImageBitmap(file);
      const side = Math.min(bitmap.width, bitmap.height);
      const canvas = document.createElement('canvas');
      canvas.width = canvas.height = AVATAR_SIZE;
      const context = canvas.getContext('2d')!;
      context.imageSmoothingQuality = 'high';
      context.drawImage(
          bitmap, (bitmap.width - side) / 2, (bitmap.height - side) / 2, side,
          side, 0, 0, AVATAR_SIZE, AVATAR_SIZE);
      bitmap.close();
      let url = canvas.toDataURL('image/png');
      if (url.length > AVATAR_MAX_LENGTH / 2) {
        url = canvas.toDataURL('image/jpeg', 0.9);
      }
      if (this.edit_) {
        this.edit_ = {...this.edit_, avatar: url};
        this.editError_ = '';
      }
    } catch {
      this.editError_ = 'Could not read that image';
    }
  }

  protected onColorClick_(e: Event) {
    const color = (e.currentTarget as HTMLElement).dataset['color'];
    if (this.edit_ && color) {
      this.edit_ = {...this.edit_, color};
    }
  }

  protected onPersistSessionCookiesChange_(e: CustomEvent<boolean>) {
    if (this.edit_) {
      this.edit_ = {...this.edit_, persistSessionCookies: e.detail};
    }
  }

  protected onKeepChange_(e: CustomEvent<boolean>) {
    if (this.edit_) {
      this.edit_ = {...this.edit_, keep: e.detail};
    }
  }

  private slug_(name: string): string {
    return name.toLowerCase()
        .normalize('NFKD')
        .replace(/[^a-z0-9]+/g, '-')
        .replace(/^-+|-+$/g, '')
        .slice(0, 64)
        .replace(/-+$/, '');
  }

  protected onEditCancelClick_() {
    this.$.editDialog.cancel();
  }

  protected onEditClose_() {
    // A late 'close' must not clear state of a dialog that was reopened.
    if (!this.$.editDialog.open) {
      this.edit_ = null;
    }
  }

  protected async onEditSaveClick_() {
    const edit = this.edit_;
    if (!edit) {
      return;
    }
    const identity: AgentIdentity = {
      name: edit.name.trim(),
      color: edit.color,
      avatar: edit.avatar.trim(),
      persistSessionCookies: edit.persistSessionCookies,
      keep: edit.keep,
    };
    if (edit.throwaway) {
      delete identity.avatar;
      delete identity.persistSessionCookies;
      delete identity.keep;
    }
    try {
      if (edit.id) {
        identity.name = identity.name || edit.id;
        await this.browserProxy_.updateAgent(edit.id, identity);
      } else {
        if (!ID_PATTERN.test(edit.newId)) {
          this.editError_ = 'Use a-z, 0-9 and single dashes';
          return;
        }
        if (!identity.name) {
          delete identity.name;
        }
        if (!identity.avatar) {
          delete identity.avatar;
        }
        await this.browserProxy_.createAgent(edit.newId, identity);
      }
    } catch (err) {
      this.editError_ = String(err);
      return;
    }
    this.$.editDialog.close();
    await this.refresh_();
  }

  // Cleanup of idle agents ---------------------------------------------------

  protected async onCleanupClick_() {
    this.$.toolbarMenu.close();
    this.cleanupAgents_ = null;
    this.cleanupError_ = '';
    this.cleanupDaysValid_ = true;
    await this.updateComplete;
    this.$.cleanupDialog.showModal();
    await this.previewCleanup_();
  }

  protected async previewCleanup_() {
    const request = ++this.cleanupRequest_;
    this.cleanupAgents_ = null;
    this.cleanupError_ = '';
    try {
      const result = await this.browserProxy_.deleteIdleAgents(
          this.cleanupDays_, /*dryRun=*/ true);
      if (request === this.cleanupRequest_) {
        this.cleanupAgents_ = result.agents;
      }
    } catch (e) {
      if (request === this.cleanupRequest_) {
        this.cleanupError_ = String(e);
      }
    }
  }

  protected onCleanupDaysInput_(e: Event) {
    const text = (e.target as HTMLInputElement).value.trim();
    const days = Number(text);
    this.cleanupDaysValid_ =
        /^[0-9]+$/.test(text) && days >= 1 && days <= 3650;
    if (!this.cleanupDaysValid_) {
      // Nothing to confirm while the field doesn't hold a valid age.
      ++this.cleanupRequest_;
      this.cleanupAgents_ = null;
      return;
    }
    if (days !== this.cleanupDays_ || this.cleanupAgents_ === null) {
      this.cleanupDays_ = days;
      this.previewCleanup_();
    }
  }

  protected onCleanupCancelClick_() {
    this.$.cleanupDialog.cancel();
  }

  protected onCleanupConfirmClick_() {
    // Deletes only the agents the list showed; each is checked again, so one
    // that was used or marked Keep meanwhile stays.
    const ids = (this.cleanupAgents_ ?? []).map(agent => agent.id);
    if (!this.cleanupDaysValid_ || !ids.length) {
      return;
    }
    this.$.cleanupDialog.close();
    this.act_(this.browserProxy_.deleteIdleAgents(
        this.cleanupDays_, /*dryRun=*/ false, ids));
  }

  // Delete -----------------------------------------------------------------

  protected async onMenuDeleteClick_() {
    this.$.menu.close();
    this.deleteAgent_ = this.menuAgent_;
    if (this.deleteAgent_) {
      await this.updateComplete;
      this.$.deleteDialog.showModal();
    }
  }

  protected onDeleteCancelClick_() {
    this.$.deleteDialog.cancel();
  }

  protected onDeleteClose_() {
    // A late 'close' must not clear state of a dialog that was reopened.
    if (!this.$.deleteDialog.open) {
      this.deleteAgent_ = null;
    }
  }

  protected onDeleteConfirmClick_() {
    const agent = this.deleteAgent_;
    this.$.deleteDialog.close();
    if (agent) {
      this.act_(this.browserProxy_.deleteAgent(agent.id));
    }
  }

  // Open tab as agent ------------------------------------------------------

  protected onOpenTabClick_(e: Event) {
    this.showOpenDialog_(this.agentFor_(e));
  }

  private async showOpenDialog_(agent: Agent|null) {
    this.openAgent_ = agent;
    if (!this.openAgent_) {
      return;
    }
    this.openUrl_ = '';
    this.openError_ = '';
    await this.updateComplete;
    this.$.openDialog.showModal();
  }

  protected onUrlValueChanged_(e: CustomEvent<{value: string}>) {
    this.openUrl_ = e.detail.value;
    this.openError_ = '';
  }

  protected onOpenCancelClick_() {
    this.$.openDialog.cancel();
  }

  protected onOpenClose_() {
    // A late 'close' must not clear state of a dialog that was reopened.
    if (!this.$.openDialog.open) {
      this.openAgent_ = null;
    }
  }

  protected async onOpenConfirmClick_() {
    if (!this.openAgent_) {
      return;
    }
    try {
      await this.browserProxy_.openTab(
          this.openAgent_.id, this.openUrl_.trim());
    } catch (err) {
      this.openError_ = String(err);
      return;
    }
    this.$.openDialog.close();
    await this.refresh_();
  }

  // Sessions and tabs ------------------------------------------------------

  protected onMenuEndSessionsClick_() {
    this.$.menu.close();
    const agent = this.menuAgent_;
    if (agent) {
      this.act_(Promise.all(this.sessionsOf_(agent).map(
          session => this.browserProxy_.closeSession(session.sessionId))));
    }
  }

  protected onCloseSessionClick_(e: Event) {
    const id = (e.currentTarget as HTMLElement).dataset['session'];
    if (id) {
      this.act_(this.browserProxy_.closeSession(id));
    }
  }

  protected onCloseTabClick_(e: Event) {
    const id = (e.currentTarget as HTMLElement).dataset['target'];
    if (id) {
      this.act_(this.browserProxy_.closeTab(id));
    }
  }

  protected onTabClick_(e: Event) {
    const id = (e.currentTarget as HTMLElement).dataset['target'];
    if (id) {
      this.act_(this.browserProxy_.showTab(id));
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'agents-app': AgentsAppElement;
  }
}

customElements.define(AgentsAppElement.is, AgentsAppElement);
