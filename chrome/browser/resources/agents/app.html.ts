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
  <select id="sort" slot="center-end" class="md-select" aria-label="Sort by"
      ?hidden="${!this.agents_.length}" @change="${this.onSortChange_}">
    <option value="name" ?selected="${this.sort_ === 'name'}">
      Sort by name</option>
    <option value="recent" ?selected="${this.sort_ === 'recent'}">
      Sort by last active</option>
    <option value="memory" ?selected="${this.sort_ === 'memory'}">
      Sort by memory</option>
    <option value="cpu" ?selected="${this.sort_ === 'cpu'}">
      Sort by CPU</option>
  </select>
  <cr-button id="create" class="action-button"
      @click="${this.onCreateClick_}">
    New agent
  </cr-button>
  <cr-icon-button id="createCompact" iron-icon="cr:add" title="New agent"
      aria-label="New agent" @click="${this.onCreateClick_}">
  </cr-icon-button>
  <cr-icon-button id="help" iron-icon="cr:help" title="Quick start"
      aria-label="Quick start: connect your agent"
      @click="${this.onHelpClick_}">
  </cr-icon-button>
  <cr-icon-button id="toolbarMore" class="icon-more-vert" title="More actions"
      aria-label="More actions" @click="${this.onToolbarMoreClick_}">
  </cr-icon-button>
</cr-toolbar>

<div id="container" class="cr-scrollable">
  <!-- Toolbar shadow while the list is scrolled, as in Settings. -->
  <div class="cr-scrollable-top-shadow force-on"></div>
  <div id="content">
    ${this.error_ ? html`
      <div class="error-card">
        <cr-icon icon="cr:error"></cr-icon>
        <span>${this.error_}</span>
      </div>` : nothing}

    ${this.loaded_ && !this.agents_.length ? html`
      <div id="empty">
        <div class="empty-title">No agents yet</div>
        <div class="secondary">Connect your agent (Help, top right); it
            creates its own. Or create one with New agent.</div>
      </div>` : nothing}

    ${this.loaded_ && this.agents_.length ? html`
      <div id="list">
        ${!this.visibleAgents_().length ? html`
          <div class="list-empty secondary">No matching agents</div>` :
          nothing}

        <div id="agents">
        ${this.visibleAgents_().map(agent => html`
          <div class="agent" data-agent="${agent.id}">
            <div class="list-item">
              <div class="item-main ${agent.sessions ? 'expandable' : ''}"
                  data-agent="${agent.id}" @click="${this.onAgentToggleClick_}">
                <div class="avatar" data-color="${agent.color}">
                  ${agent.avatar ? html`<img src="${agent.avatar}" alt="">` :
                                   html`<span>${this.initial_(agent)}</span>`}
                </div>
                <div class="text two-line">
                  <div class="name">${agent.name}
                    ${agent.throwaway ? html`
                      <span class="chip" title="${this.expiryText_(agent)}">
                        Temporary</span>` : nothing}
                    ${agent.keep ? html`<span class="chip">Kept</span>` :
                                   nothing}
                  </div>
                  <div class="secondary">${this.agentDetailText_(agent)}</div>
                </div>
                ${agent.sessions ? html`
                  <cr-icon-button class="${this.isExpanded_(agent) ?
                          'icon-expand-less' : 'icon-expand-more'} expand"
                      aria-expanded="${this.isExpanded_(agent)}"
                      aria-label="Tabs of ${agent.name}">
                  </cr-icon-button>` : nothing}
              </div>
              <cr-button class="open-tab" data-agent="${agent.id}"
                  @click="${this.onOpenTabClick_}"
                  title="Open a tab with this agent's cookies and logins">
                New tab
              </cr-button>
              <div class="separator"></div>
              <cr-icon-button class="icon-more-vert trailing more"
                  data-agent="${agent.id}" title="More actions"
                  aria-label="More actions for ${agent.name}"
                  @click="${this.onMoreClick_}">
              </cr-icon-button>
            </div>
            ${this.isExpanded_(agent) ? html`
              <div class="list-frame">
                ${this.shownSessionsOf_(agent).map(session => html`
                  ${this.sessionsOf_(agent).length > 1 ? html`
                    <div class="list-item child session-item">
                      <div class="text">
                        Session ${session.sessionId}<span
                            class="secondary spacing">Opened ${
                            this.since_(session.openedAt)}</span>
                      </div>
                      <cr-button class="close-session"
                          data-session="${session.sessionId}"
                          @click="${this.onCloseSessionClick_}">
                        End session
                      </cr-button>
                    </div>` : nothing}
                  ${session.tabs.map(tab => html`
                    <div class="list-item child">
                      <div class="item-main" data-target="${tab.targetId}"
                          title="${tab.url}" @click="${this.onTabClick_}">
                        ${this.favicons_[tab.targetId] ? html`
                          <img class="favicon" alt=""
                              src="${this.favicons_[tab.targetId]}">` : html`
                          <span class="favicon placeholder"></span>`}
                        <div class="text">
                          <span class="tab-title">${
                            this.tabLabel_(tab.title, tab.url)}</span><span
                              class="secondary spacing">${
                              this.host_(tab.url)}</span>
                        </div>
                        <cr-icon-button class="subpage-arrow"
                            aria-label="Show tab ${
                              this.tabLabel_(tab.title, tab.url)}">
                        </cr-icon-button>
                      </div>
                      <div class="separator"></div>
                      <cr-icon-button class="icon-delete-gray trailing"
                          data-target="${tab.targetId}" title="Close tab"
                          aria-label="Close tab ${
                            this.tabLabel_(tab.title, tab.url)}"
                          @click="${this.onCloseTabClick_}">
                      </cr-icon-button>
                    </div>`)}
                  ${!session.tabs.length ? html`
                    <div class="list-item child">
                      <div class="text secondary">No tabs open</div>
                    </div>` : nothing}
                `)}
              </div>` : nothing}
          </div>`)}
        </div>
      </div>` : nothing}
  </div>
</div>

${this.loaded_ && this.agents_.length ? html`
  <div id="footer">
    <div id="summary">${this.summaryText_()}</div>
  </div>` : nothing}

<cr-action-menu id="toolbarMenu" role-description="Menu">
  <button class="dropdown-item" id="menuTaskManager"
      @click="${this.onTaskManagerClick_}">
    Task Manager
  </button>
  <button class="dropdown-item" id="menuCleanup"
      @click="${this.onCleanupClick_}">
    Clean up idle agents…
  </button>
</cr-action-menu>

<cr-action-menu id="menu" role-description="Menu">
  <button class="dropdown-item" id="menuEndSessions"
      ?hidden="${!this.menuAgent_?.sessions}"
      @click="${this.onMenuEndSessionsClick_}">
    ${(this.menuAgent_?.sessions ?? 0) > 1 ? 'End all sessions' :
                                             'End session'}
  </button>
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
    ${!this.edit_?.id ? html`
      <cr-input id="idInput" label="Id" .value="${this.edit_?.newId || ''}"
          maxlength="64" placeholder="research-assistant" spellcheck="false"
          ?invalid="${!!this.editError_}" error-message="${this.editError_}"
          @value-changed="${this.onIdValueChanged_}">
      </cr-input>
      <div class="hint">Agents use the id in MCP calls. It can't be changed
          later.</div>` : nothing}
    <div class="field-label">Color</div>
    <div class="swatches" role="radiogroup" aria-label="Color">
      ${this.colors.map(color => html`
        <button class="swatch" data-color="${color}" role="radio"
            aria-label="${color}" title="${color}"
            aria-checked="${color === this.edit_?.color}"
            @click="${this.onColorClick_}"></button>`)}
    </div>
    ${!this.edit_?.throwaway ? html`
    <div class="field-label">Avatar</div>
    <div class="avatar-row">
      <div class="avatar" data-color="${this.edit_?.color || 'grey'}">
        ${this.edit_?.avatar ? html`<img src="${this.edit_.avatar}" alt="">` :
            html`${(this.edit_?.name || this.edit_?.newId || '?')
                .slice(0, 1).toUpperCase()}`}
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
    <div class="toggle-row">
      <div class="text">
        <div id="persistSessionCookiesLabel">Keep session cookies after
            restart</div>
        <div class="secondary">Like "Continue where you left off": sites
            stay signed in. Applies at the next start of Shuttle.</div>
      </div>
      <cr-toggle id="persistSessionCookiesToggle"
          aria-labelledby="persistSessionCookiesLabel"
          ?checked="${this.edit_?.persistSessionCookies ?? true}"
          @change="${this.onPersistSessionCookiesChange_}">
      </cr-toggle>
    </div>
    <div class="toggle-row">
      <div class="text">
        <div id="keepLabel">Keep this agent</div>
        <div class="secondary">Never removed by Clean up. Turn on for agents
            that hold a real login.</div>
      </div>
      <cr-toggle id="keepToggle" aria-labelledby="keepLabel"
          ?checked="${this.edit_?.keep ?? false}"
          @change="${this.onKeepChange_}">
      </cr-toggle>
    </div>` : nothing}
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
<cr-dialog id="cleanupDialog" close-text="Close">
  <div slot="title">Clean up idle agents</div>
  <div slot="body">
    <div class="dialog-text">
      Delete agents with no open session and no activity for
      <input id="cleanupDays" type="number" min="1" max="3650" step="1"
          aria-invalid="${!this.cleanupDaysValid_}"
          aria-describedby="cleanupDaysHint"
          .value="${String(this.cleanupDays_)}"
          @input="${this.onCleanupDaysInput_}"> days. Their cookies, logins
      and other site data are deleted. Agents marked Keep, agents with an open
      session and temporary agents are never touched.
    </div>
    <div id="cleanupDaysHint"
        class="secondary ${this.cleanupDaysValid_ ? '' : 'error'}">
      ${this.cleanupDaysValid_ ? html`Agents that existed before activity was
        recorded count as active from the day of that update.` :
        html`Enter a whole number of days from 1 to 3650.`}
    </div>
    ${this.cleanupError_ ? html`
      <div class="dialog-text error">${this.cleanupError_}</div>` : nothing}
    ${this.cleanupDaysValid_ && this.cleanupAgents_ === null &&
      !this.cleanupError_ ? html`
      <div class="secondary">Checking…</div>` : nothing}
    ${this.cleanupAgents_ && !this.cleanupAgents_.length ? html`
      <div class="secondary">Nothing to delete.</div>` : nothing}
    ${this.cleanupAgents_ && this.cleanupAgents_.length ? html`
      <div class="dialog-text">These ${this.cleanupAgents_.length} will be
          deleted:</div>
      <ul id="cleanupList">
        ${this.cleanupAgents_.map(agent => html`
          <li>${agent.name} <span class="secondary">(${agent.id}) ·
              last active ${this.since_(agent.lastActiveAt)}</span></li>`)}
      </ul>` : nothing}
  </div>
  <div slot="button-container">
    <cr-button class="cancel-button" @click="${this.onCleanupCancelClick_}">
      Cancel
    </cr-button>
    <cr-button class="action-button" id="cleanupConfirm"
        ?disabled="${!this.cleanupDaysValid_ || !this.cleanupAgents_?.length}"
        @click="${this.onCleanupConfirmClick_}">
      Delete ${this.cleanupAgents_?.length || ''}
    </cr-button>
  </div>
</cr-dialog>
<cr-dialog id="helpDialog" close-text="Close" show-close-button
    @click="${this.onHelpDialogClick_}">
  <div slot="title">Quick start</div>
  <div slot="body">
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
  </div>
  </div>
  <div slot="button-container">
    <cr-button class="action-button" @click="${this.onHelpCloseClick_}">
      Done
    </cr-button>
  </div>
</cr-dialog>
<!--_html_template_end_-->`;
}
