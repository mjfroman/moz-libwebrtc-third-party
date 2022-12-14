// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file was generated by:
//   ./tools/json_schema_compiler/compiler.py.

/** @fileoverview Interface for bluetooth that can be overriden. */

/** @interface */
function Bluetooth() {}

Bluetooth.prototype = {
  /**
   * Get information about the Bluetooth adapter.
   * @param {function(!chrome.bluetooth.AdapterState): void} callback Called
   *     with an AdapterState object describing the adapter state.
   * @see https://developer.chrome.com/extensions/bluetooth#method-getAdapterState
   */
  getAdapterState: function(callback) {},

  /**
   * Get information about a Bluetooth device known to the system.
   * @param {string} deviceAddress Address of device to get.
   * @param {function(!chrome.bluetooth.Device): void} callback Called with the
   *     Device object describing the device.
   * @see https://developer.chrome.com/extensions/bluetooth#method-getDevice
   */
  getDevice: function(deviceAddress, callback) {},

  /**
   * Get a list of Bluetooth devices known to the system, including paired and
   * recently discovered devices.
   * @param {?chrome.bluetooth.BluetoothFilter|undefined} filter Some criteria
   *     to filter the list of returned bluetooth devices. If the filter is not
   *     set or set to <code>{}</code>, returned device list will contain all
   *     bluetooth devices. Right now this is only supported in ChromeOS, for
   *     other platforms, a full list is returned.
   * @param {function(!Array<!chrome.bluetooth.Device>): void} callback Called
   *     when the search is completed.
   * @see https://developer.chrome.com/extensions/bluetooth#method-getDevices
   */
  getDevices: function(filter, callback) {},

  /**
   * <p>Start discovery. Newly discovered devices will be returned via the
   * onDeviceAdded event. Previously discovered devices already known to the
   * adapter must be obtained using getDevices and will only be updated using
   * the |onDeviceChanged| event if information about them
   * changes.</p><p>Discovery will fail to start if this application has already
   * called startDiscovery.  Discovery can be resource intensive: stopDiscovery
   * should be called as soon as possible.</p>
   * @param {function(): void=} callback Called to indicate success or failure.
   * @see https://developer.chrome.com/extensions/bluetooth#method-startDiscovery
   */
  startDiscovery: function(callback) {},

  /**
   * Stop discovery.
   * @param {function(): void=} callback Called to indicate success or failure.
   * @see https://developer.chrome.com/extensions/bluetooth#method-stopDiscovery
   */
  stopDiscovery: function(callback) {},
};

/**
 * Fired when the state of the Bluetooth adapter changes.
 * @type {!ChromeEvent}
 * @see https://developer.chrome.com/extensions/bluetooth#event-onAdapterStateChanged
 */
Bluetooth.prototype.onAdapterStateChanged;

/**
 * Fired when information about a new Bluetooth device is available.
 * @type {!ChromeEvent}
 * @see https://developer.chrome.com/extensions/bluetooth#event-onDeviceAdded
 */
Bluetooth.prototype.onDeviceAdded;

/**
 * Fired when information about a known Bluetooth device has changed.
 * @type {!ChromeEvent}
 * @see https://developer.chrome.com/extensions/bluetooth#event-onDeviceChanged
 */
Bluetooth.prototype.onDeviceChanged;

/**
 * Fired when a Bluetooth device that was previously discovered has been out of
 * range for long enough to be considered unavailable again, and when a paired
 * device is removed.
 * @type {!ChromeEvent}
 * @see https://developer.chrome.com/extensions/bluetooth#event-onDeviceRemoved
 */
Bluetooth.prototype.onDeviceRemoved;
