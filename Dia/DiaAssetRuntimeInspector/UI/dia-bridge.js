(function() {
    'use strict';

    var topicHandlers = {};

    window.diaBridge = {
        onDataChanged: function(topic, callback) {
            if (!topicHandlers[topic]) topicHandlers[topic] = [];
            topicHandlers[topic].push(callback);
        },
        sendRequest: function(type, data) {
            if (window.parent && window.parent !== window) {
                var reqId = 'dia-' + Date.now() + '-' + Math.random().toString(36).substr(2, 6);
                window.parent.postMessage({
                    __diaFromFrame: true,
                    payload: { type: type, reqId: reqId, data: data || {} }
                }, '*');
            }
        }
    };

    window.addEventListener('message', function(e) {
        var msg = e.data;
        if (!msg || !msg.__dia) return;

        var topic = msg.topic;
        var data = msg.data;
        if (topic && topicHandlers[topic]) {
            topicHandlers[topic].forEach(function(fn) {
                try { fn(data); } catch(ex) { console.warn('diaBridge handler error:', topic, ex); }
            });
        }
    });
})();
