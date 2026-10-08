// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertNotReached} from '//resources/js/assert.js';
import type {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

type Constructor<T> = new (...args: any[]) => T;

export interface ToolbarActionMixinInterface<T> {
  state: T;
  // Set if the TrackedElementManager indicates the element should be
  // highlighted.
  trackedHighlighted: boolean;
  getElementId(state: T): string|undefined;
  getSecondaryElementId(): string|undefined;
  getMimeType(): string;
  getItemId(): string;
  isDraggable(): boolean;
  moveItemBy(delta: number): void;
  onDragstart(e: DragEvent): void;
  onDragend(e: DragEvent): void;
  onKeydown(e: KeyboardEvent): void;
}

/**
 * A mixin for individual WebUI toolbar action buttons (such as pinned toolbar
 * actions and extensions).
 *
 * It provides drag-and-drop reordering and keyboard reordering support.
 */
export const ToolbarActionMixin =
    <T, BaseClass extends Constructor<CrLitElement>>(
        superClass: BaseClass, initialState: T): BaseClass&
    Constructor<ToolbarActionMixinInterface<T>> => {
      class ToolbarActionMixin extends superClass implements
          ToolbarActionMixinInterface<T> {
        static get properties() {
          return {
            state: {type: Object},
            trackedHighlighted: {type: Boolean},
          };
        }

        accessor state: T = initialState;
        accessor trackedHighlighted: boolean = false;

        /**
         * Returns the primary ElementIdentifier for element tracking, or null/empty
         * if this item does not support element tracking.
         */
        getElementId(_state: T): string|undefined {
          return undefined;
        }

        /**
         * Optionally returns a secondary identifier for element tracking (e.g.
         * 'ext:id').
         */
        getSecondaryElementId(): string|undefined {
          return undefined;
        }

        // Delegate focus to the internal button element. Custom elements do not
        // automatically delegate focus to their shadow DOM content unless
        // configured with delegatesFocus, which Lit doesn't do by default for
        // wrapper elements.
        override focus() {
          const button = this.shadowRoot?.querySelector(
                             'cr-icon-button, cr-button') as HTMLElement;
          if (button) {
            button.focus();
          }
        }

        getMimeType(): string {
          assertNotReached();
        }

        getItemId(): string {
          assertNotReached();
        }

        isDraggable(): boolean {
          return assertNotReached();
        }

        moveItemBy(_delta: number) {
          assertNotReached();
        }

        onDragstart(e: DragEvent) {
          if (!this.isDraggable() || !e.dataTransfer) {
            e.preventDefault();
            return;
          }
          const payload = JSON.stringify({
            itemId: this.getItemId(),
          });
          e.dataTransfer.setData(this.getMimeType(), payload);
          e.dataTransfer.effectAllowed = 'move';

          this.fire('toolbar-action-drag-start', {itemId: this.getItemId()});
        }

        onDragend(e: DragEvent) {
          this.fire('toolbar-action-drag-end', {
            itemId: this.getItemId(),
            dropEffect: e.dataTransfer?.dropEffect,
          });
        }

        onKeydown(e: KeyboardEvent) {
          const isModifier = e.ctrlKey || e.metaKey;
          if (!isModifier ||
              (e.key !== 'ArrowLeft' && e.key !== 'ArrowRight')) {
            return;
          }
          e.preventDefault();
          e.stopPropagation();
          const delta = e.key === 'ArrowLeft' ? -1 : 1;

          // Notify the parent container that a keyboard reorder is occurring.
          // The container will use this to lock and restore focus to this
          // action button after the DOM updates with the new order.
          this.fire(
              'toolbar-action-keyboard-reorder', {itemId: this.getItemId()});
          this.moveItemBy(delta);
        }
      }

      return ToolbarActionMixin;
    };
