// Copyright 2026 The Agent Chrome Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/strings.m.js';
import 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.js';
import 'chrome://resources/cr_elements/cr_toolbar/cr_toolbar.js';
import 'chrome://resources/cr_elements/icons.html.js';

import type {CrActionMenuElement} from 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import type {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './app.css.js';
import {getHtml} from './app.html.js';
import type {Agent, AgentIdentity, BrowserProxy, Session} from './browser_proxy.js';
import {AGENT_COLORS, BrowserProxyImpl} from './browser_proxy.js';

// How often the page refreshes while visible (agents change things too).
const REFRESH_MS = 2000;

// Agent id rule of the MCP server (kebab-case, max 64).
const ID_PATTERN = /^[a-z0-9]+(-[a-z0-9]+)*$/;

export interface EditState {
  // Agent id; '' while creating.
  id: string;
  newId: string;
  name: string;
  color: string;
  avatar: string;
}

export interface AgentsAppElement {
  $: {
    deleteDialog: CrDialogElement,
    editDialog: CrDialogElement,
    menu: CrActionMenuElement,
    openDialog: CrDialogElement,
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

  protected readonly colors = AGENT_COLORS;
  private browserProxy_: BrowserProxy = BrowserProxyImpl.getInstance();
  private timer_: number|null = null;
  // Agent whose "more actions" menu is open.
  private menuAgent_: Agent|null = null;
  private onVisibilityChange_ = () => this.schedule_();

  override connectedCallback() {
    super.connectedCallback();
    document.addEventListener('visibilitychange', this.onVisibilityChange_);
    this.refresh_();
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

  protected visibleAgents_(): Agent[] {
    const query = this.search_.trim().toLowerCase();
    if (!query) {
      return this.agents_;
    }
    return this.agents_.filter(
        a => a.name.toLowerCase().includes(query) || a.id.includes(query));
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
    this.showEditDialog_({id: '', newId: '', name: '', color, avatar: ''});
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
      });
    }
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
    if (this.edit_) {
      this.edit_ = {...this.edit_, avatar: e.detail.value};
    }
  }

  protected onColorClick_(e: Event) {
    const color = (e.currentTarget as HTMLElement).dataset['color'];
    if (this.edit_ && color) {
      this.edit_ = {...this.edit_, color};
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
    };
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

  protected async onOpenTabClick_(e: Event) {
    this.openAgent_ = this.agentFor_(e);
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

  protected onCloseSessionClick_(e: Event) {
    const id = (e.currentTarget as HTMLElement).dataset['session'];
    if (id) {
      this.act_(this.browserProxy_.closeSession(id));
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
