// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {SettingsClearBrowsingDataDialogElement} from './clear_browsing_data_dialog.js';

export function getHtml(this: SettingsClearBrowsingDataDialogElement) {
  return html`<!--_html_template_start_-->
<cr-dialog id="deleteBrowsingDataDialog" close-text="$i18n{close}"
    show-on-attach ignore-popstate ignore-enter-key>
  <div slot="title" class="dialog-title">$i18n{clearBrowsingData}</div>
  <div slot="header">
    <settings-clear-browsing-data-time-picker id="timePicker"
        @selected-time-period-change="${this.onSelectedTimePeriodChange_}">
    </settings-clear-browsing-data-time-picker>
  </div>
  <div slot="body">
    <div id="checkboxContainer">
      ${this.expandedBrowsingDataTypeOptionsList_.map(item => html`
        <settings-checkbox pref-key="${item.prefKey}" no-set-pref
            .subLabelHtml="${item.subLabel || ''}"
            ?disabled="${this.isDeletionInProgress_}"
            @sub-label-link-clicked="${this.onSubLabelLinkClicked_}">
          <div class="checkbox-title">${item.label}</div>
        </settings-checkbox>
      `)}
      <div id="moreOptionsList">
        ${this.dataTypesExpanded_ ? html`
          ${this.moreBrowsingDataTypeOptionsList_.map(item => html`
            <settings-checkbox pref-key="${item.prefKey}" no-set-pref
                .subLabelHtml="${item.subLabel || ''}"
                ?disabled="${this.isDeletionInProgress_}"
                @sub-label-link-clicked="${this.onSubLabelLinkClicked_}">
              <div class="checkbox-title">${item.label}</div>
            </settings-checkbox>
          `)}
        ` : ''}
      </div>
    </div>
    <cr-button id="showMoreButton" @click="${this.onShowMoreClick_}"
        ?hidden="${this.shouldHideShowMoreButton_()}">
      $i18n{clearBrowsingDataShowMore}
      <cr-icon icon="cr:keyboard-arrow-down" aria-hidden="true"
          role="presentation">
      </cr-icon>
    </cr-button>
  </div>
  <div slot="button-container" class="row-aligned">
    <div id="spinner" class="spinner"
        ?hidden="${!this.isDeletionInProgress_}"></div>
    <cr-button id="cancelButton" class="cancel-button"
        @click="${this.onCancelClick_}"
        ?disabled="${this.isDeletionInProgress_}" autofocus>
      $i18n{cancel}
    </cr-button>
    <cr-button id="deleteButton" class="action-button"
        @click="${this.onDeleteBrowsingDataClick_}"
        ?disabled="${this.shouldDisableDeleteButton_()}">
      ${this.computeDeleteButtonLabel_()}
    </cr-button>
    <!-- The alert must be inside the dialog for it to be read while the
         dialog is open. -->
    <div id="deletingDataAlert" role="alert">
      ${this.deletingDataAlertString_}
    </div>
  </div>
</cr-dialog>

${this.showHistoryDeletionDialog_ ? html`
  <settings-history-deletion-dialog id="historyNotice"
      @close="${this.onHistoryDeletionDialogClose_}">
  </settings-history-deletion-dialog>
` : ''}

<!--_html_template_end_-->`;
}
