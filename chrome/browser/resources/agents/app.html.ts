// Copyright 2026 Silkweave
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html, nothing} from '//resources/lit/v3_0/lit.rollup.js';

import type {AgentsAppElement} from './app.js';

export function getHtml(this: AgentsAppElement) {
  return html`<!--_html_template_start_-->
<cr-toolbar id="toolbar" page-name="Agents" search-prompt="Search agents"
    clear-label="Clear search" menu-label="" autofocus
    @search-changed="${this.onSearchChanged_}">
  <cr-button id="create" class="action-button" @click="${this.onCreateClick_}">
    New agent
  </cr-button>
</cr-toolbar>

<div id="container">
  <div id="content">
    ${this.error_ ? html`
      <div class="card error-card">
        <cr-icon icon="cr:error"></cr-icon>
        <span>${this.error_}</span>
      </div>` : nothing}

    <div class="card" id="quickStart">
      <button class="row quick-start-header" id="quickStartToggle"
          aria-expanded="${this.quickStartOpen_()}"
          @click="${this.onQuickStartToggleClick_}">
        <div class="text">
          <div class="name">Quick start</div>
          <div class="secondary">Connect your agent to Shuttle over MCP</div>
        </div>
        <cr-icon icon="${this.quickStartOpen_() ? 'cr:keyboard-arrow-up' :
                                                  'cr:keyboard-arrow-down'}">
        </cr-icon>
      </button>
      ${this.quickStartOpen_() ? html`
        <div class="quick-start-body">
          <div class="step">
            <div class="step-title">1. Add Shuttle to your agent</div>
            <div class="secondary">Claude Code:</div>
            <div class="code-row">
              <code id="claudeCommand">${this.claudeCommand_()}</code>
              <cr-button class="copy" data-copy="claude" @click="${this.onCopyClick_}">
              ${this.copied_ === 'claude' ? 'Copied' : 'Copy'}
            </cr-button>
            </div>
            <div class="secondary">Codex:</div>
            <div class="code-row">
              <code id="codexCommand">${this.codexCommand_()}</code>
              <cr-button class="copy" data-copy="codex" @click="${this.onCopyClick_}">
              ${this.copied_ === 'codex' ? 'Copied' : 'Copy'}
            </cr-button>
            </div>
            <div class="secondary">Other MCP clients: add a Streamable HTTP
                server with this URL.</div>
            <div class="code-row">
              <code id="mcpUrl">${this.mcpUrl_}</code>
              <cr-button class="copy" data-copy="url" @click="${this.onCopyClick_}">
              ${this.copied_ === 'url' ? 'Copied' : 'Copy'}
            </cr-button>
            </div>
          </div>
          <div class="step">
            <div class="step-title">2. Ask your agent to use it</div>
            <div class="secondary">Restart the agent so it loads the server,
                then try:</div>
            <div class="code-row">
              <code id="examplePrompt" class="prompt">${this.examplePrompt_()}</code>
              <cr-button class="copy" data-copy="prompt" @click="${this.onCopyClick_}">
              ${this.copied_ === 'prompt' ? 'Copied' : 'Copy'}
            </cr-button>
            </div>
          </div>
          <div class="step secondary">Keep Shuttle running while your agent
              works. Its tabs show up in a group with its name and color, and
              below on this page. Sites it signs in to stay signed in for that
              agent only.</div>
        </div>` : nothing}
    </div>

    ${this.loaded_ && !this.agents_.length ? html`
      <div id="empty">
        <div class="empty-title">No agents yet</div>
        <div class="secondary">Connect your agent (Quick start above); it
            creates its own. Or create one with New agent.</div>
      </div>` : nothing}

    ${this.loaded_ && this.agents_.length && !this.visibleAgents_().length ?
        html`<div id="empty"><div class="secondary">No matching agents</div>
             </div>` : nothing}

    ${this.visibleAgents_().map(agent => html`
      <div class="card agent" data-agent="${agent.id}">
        <div class="row agent-row">
          <div class="avatar" data-color="${agent.color}">
            ${agent.avatar ? html`<img src="${agent.avatar}" alt="">` :
                             html`<span>${this.initial_(agent)}</span>`}
          </div>
          <div class="text">
            <div class="name">${agent.name}</div>
            <div class="secondary">
              ${agent.id} · ${this.plural_(agent.sessions, 'session')} ·
              ${this.plural_(agent.tabs, 'tab')}
            </div>
          </div>
          <cr-button class="open-tab" data-agent="${agent.id}"
              @click="${this.onOpenTabClick_}"
              title="Open a tab with this agent's cookies and logins">
            Open tab
          </cr-button>
          <cr-icon-button class="icon-more-vert more" data-agent="${agent.id}"
              title="More actions" aria-label="More actions for ${agent.name}"
              @click="${this.onMoreClick_}">
          </cr-icon-button>
        </div>
        ${this.sessionsOf_(agent).map(session => html`
          <div class="row session-row">
            <div class="text">
              <div>Session ${session.sessionId}</div>
              <div class="secondary">
                Opened ${this.since_(session.openedAt)} ·
                ${this.plural_(session.tabs.length, 'tab')}
              </div>
            </div>
            <cr-button class="close-session" data-session="${session.sessionId}"
                @click="${this.onCloseSessionClick_}">
              End session
            </cr-button>
          </div>
          ${session.tabs.map(tab => html`
            <button class="row tab-row" data-target="${tab.targetId}"
                @click="${this.onTabClick_}" title="${tab.url}">
              <div class="text">
                <div class="tab-title">${this.tabLabel_(tab.title, tab.url)}
                </div>
                <div class="secondary">${this.host_(tab.url)}</div>
              </div>
              <cr-icon icon="cr:open-in-new"></cr-icon>
            </button>`)}
        `)}
      </div>`)}
  </div>
</div>

<cr-action-menu id="menu" role-description="Menu">
  <button class="dropdown-item" id="menuEdit" @click="${this.onMenuEditClick_}">
    Edit
  </button>
  <button class="dropdown-item" id="menuDelete"
      @click="${this.onMenuDeleteClick_}">
    Delete
  </button>
</cr-action-menu>

<cr-dialog id="editDialog" close-text="Close" @close="${this.onEditClose_}">
  <div slot="title">
    ${this.edit_?.id ? 'Edit agent' : 'New agent'}
  </div>
  <div slot="body">
    <cr-input id="nameInput" label="Name" .value="${this.edit_?.name || ''}"
        maxlength="64" placeholder="${this.edit_?.id || 'Research assistant'}"
        @value-changed="${this.onNameValueChanged_}" autofocus>
    </cr-input>
    ${this.edit_?.id ? nothing : html`
      <cr-input id="idInput" label="Id" .value="${this.edit_?.newId || ''}"
          maxlength="64" placeholder="research-assistant" spellcheck="false"
          ?invalid="${!!this.editError_}" error-message="${this.editError_}"
          @value-changed="${this.onIdValueChanged_}">
      </cr-input>
      <div class="hint">Agents use the id in MCP calls. It can't be changed
          later.</div>`}
    <div class="field-label">Color</div>
    <div class="swatches" role="radiogroup" aria-label="Color">
      ${this.colors.map(color => html`
        <button class="swatch" data-color="${color}" role="radio"
            aria-label="${color}" title="${color}"
            aria-checked="${color === this.edit_?.color}"
            @click="${this.onColorClick_}"></button>`)}
    </div>
    <div class="field-label">Avatar</div>
    <div class="avatar-row">
      <div class="avatar" data-color="${this.edit_?.color || 'grey'}">
        ${this.edit_?.avatar ? html`<img src="${this.edit_.avatar}" alt="">` :
            (this.edit_?.name || this.edit_?.newId || '?')
                .slice(0, 1).toUpperCase()}
      </div>
      <cr-button id="avatarChoose" @click="${this.onAvatarChooseClick_}">
        Choose image…
      </cr-button>
      ${this.edit_?.avatar ? html`
        <cr-button id="avatarRemove" @click="${this.onAvatarRemoveClick_}">
          Remove
        </cr-button>` : nothing}
      <input id="avatarFile" type="file" accept="image/*" hidden
          @change="${this.onAvatarFileChange_}">
    </div>
    <cr-input id="avatarInput" label="Or image URL (optional)"
        .value="${this.isDataAvatar_() ? '' : (this.edit_?.avatar || '')}"
        placeholder="${this.isDataAvatar_() ? 'Image from file' :
                                              'https://…'}"
        spellcheck="false"
        ?invalid="${!!this.edit_?.id && !!this.editError_}"
        error-message="${this.editError_}"
        @value-changed="${this.onAvatarValueChanged_}">
    </cr-input>
  </div>
  <div slot="button-container">
    <cr-button class="cancel-button" @click="${this.onEditCancelClick_}">
      Cancel
    </cr-button>
    <cr-button class="action-button" id="editSave"
        @click="${this.onEditSaveClick_}">
      ${this.edit_?.id ? 'Save' : 'Create'}
    </cr-button>
  </div>
</cr-dialog>

<cr-dialog id="openDialog" close-text="Close" @close="${this.onOpenClose_}">
  <div slot="title">Open a tab as ${this.openAgent_?.name || ''}</div>
  <div slot="body">
    <div class="dialog-text">The tab uses this agent's cookies and logins,
        for example to sign in for it, and sits with its other tabs.</div>
    <cr-input id="urlInput" label="URL (optional)" .value="${this.openUrl_}"
        placeholder="https://example.com" spellcheck="false"
        ?invalid="${!!this.openError_}" error-message="${this.openError_}"
        @value-changed="${this.onUrlValueChanged_}" autofocus>
    </cr-input>
  </div>
  <div slot="button-container">
    <cr-button class="cancel-button" @click="${this.onOpenCancelClick_}">
      Cancel
    </cr-button>
    <cr-button class="action-button" id="openConfirm"
        @click="${this.onOpenConfirmClick_}">
      Open
    </cr-button>
  </div>
</cr-dialog>

<cr-dialog id="deleteDialog" close-text="Close"
    @close="${this.onDeleteClose_}">
  <div slot="title">Delete ${this.deleteAgent_?.name || ''}?</div>
  <div slot="body">
    <div class="dialog-text">
      The agent's cookies, logins and other site data are deleted.
      ${this.deleteAgent_?.sessions ?
          `Its ${this.plural_(this.deleteAgent_.sessions, 'session')} and ` +
              `${this.plural_(this.deleteAgent_.tabs, 'tab')} close.` :
          ''}
    </div>
  </div>
  <div slot="button-container">
    <cr-button class="cancel-button" @click="${this.onDeleteCancelClick_}">
      Cancel
    </cr-button>
    <cr-button class="action-button" id="deleteConfirm"
        @click="${this.onDeleteConfirmClick_}">
      Delete
    </cr-button>
  </div>
</cr-dialog>
<!--_html_template_end_-->`;
}
