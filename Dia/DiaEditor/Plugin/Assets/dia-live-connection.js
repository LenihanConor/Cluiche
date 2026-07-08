/**
 * dia-live-connection.js
 *
 * Shared live-connection wiring for vanilla HTML editor plugins.
 * Handles the EIRM-003 bridge contract so each plugin doesn't repeat it.
 *
 * Usage:
 *   <script src="../dia-live-connection.js"></script>
 *   <div id="disconnected-overlay"></div>  <!-- optional; injected if absent -->
 *   <script>
 *     DiaLiveConnection.init({
 *       prefix: 'my_inspector',
 *       overlayId: 'disconnected-overlay',        // optional, default below
 *       onConnected:    function() { ... },        // optional
 *       onDisconnected: function() { ... },        // optional
 *       onData: function(topic, data) { ... },     // receives all non-connection topics
 *     });
 *   </script>
 *
 * The helper:
 *   - Sets window.DiaEditor_onDataChanged(msg) with the correct {topic, data} signature
 *   - Listens for postMessage events with env.__dia === true (frame path)
 *   - Shows/hides the disconnected overlay on connection state changes
 *   - Sends <prefix>.get_connection_state on load to prime the initial state
 *   - Starts in the disconnected state until the C++ side responds
 */
(function (global) {
    'use strict';

    var _opts = null;

    function _sendRequest(type, data) {
        if (global.parent && global.parent !== global) {
            global.parent.postMessage({
                __diaFromFrame: true,
                payload: { type: type, reqId: null, data: data || {} }
            }, '*');
        }
    }

    function _setConnected(connected) {
        var overlayId = (_opts && _opts.overlayId) || 'disconnected-overlay';
        var overlay = document.getElementById(overlayId);
        if (!overlay) {
            overlay = document.createElement('div');
            overlay.id = overlayId;
            overlay.style.cssText = [
                'display:none',
                'position:absolute',
                'inset:0',
                'background:rgba(30,30,30,0.92)',
                'z-index:10',
                'flex-direction:column',
                'align-items:center',
                'justify-content:center',
                'gap:6px',
                'text-align:center',
                'padding:20px'
            ].join(';');
            var title = document.createElement('div');
            title.style.cssText = 'font-size:13px;color:#f44747;font-weight:600';
            title.textContent = 'No game connected';
            var hint = document.createElement('div');
            hint.style.cssText = 'font-size:11px;color:#808080';
            hint.textContent = 'Use the Game Connection panel in the toolbar to connect.';
            overlay.appendChild(title);
            overlay.appendChild(hint);
            var body = document.body || document.documentElement;
            body.insertBefore(overlay, body.firstChild);
        }

        if (connected) {
            overlay.style.display = 'none';
            if (_opts && typeof _opts.onConnected === 'function') _opts.onConnected();
        } else {
            overlay.style.display = 'flex';
            if (_opts && typeof _opts.onDisconnected === 'function') _opts.onDisconnected();
        }
    }

    function _dispatch(topic, data) {
        if (!_opts) return;
        var connTopic = _opts.prefix + '.connection_state';
        if (topic === connTopic) {
            if (data && data.connected !== undefined) _setConnected(data.connected);
        } else if (typeof _opts.onData === 'function') {
            _opts.onData(topic, data);
        }
    }

    function init(opts) {
        _opts = opts || {};

        // Direct call from C++ via CallJavaScript("DiaEditor_onDataChanged", json)
        global.DiaEditor_onDataChanged = function (msg) {
            _dispatch(msg.topic, msg.data);
        };

        // Frame postMessage path (same data, different routing)
        global.addEventListener('message', function (e) {
            var env = e.data;
            if (env && env.__dia === true && typeof env.topic === 'string') {
                _dispatch(env.topic, env.data);
            }
        });

        // Start disconnected; query C++ for real state
        _setConnected(false);
        _sendRequest(_opts.prefix + '.get_connection_state', {});
    }

    global.DiaLiveConnection = { init: init, sendRequest: _sendRequest };
}(window));
