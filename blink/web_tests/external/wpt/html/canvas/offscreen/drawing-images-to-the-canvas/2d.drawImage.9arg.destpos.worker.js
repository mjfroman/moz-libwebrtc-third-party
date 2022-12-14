// META: timeout=long
// DO NOT EDIT! This test has been generated by /html/canvas/tools/gentest.py.
// OffscreenCanvas test in a worker:2d.drawImage.9arg.destpos
// Description:
// Note:

importScripts("/resources/testharness.js");
importScripts("/html/canvas/resources/canvas-tests.js");

var t = async_test("");
var t_pass = t.done.bind(t);
var t_fail = t.step_func(function(reason) {
    throw reason;
});
t.step(function() {

var canvas = new OffscreenCanvas(100, 50);
var ctx = canvas.getContext('2d');

ctx.fillStyle = '#f00';
ctx.fillRect(0, 0, 100, 50);
var promise1 = new Promise(function(resolve, reject) {
    var xhr = new XMLHttpRequest();
    xhr.open("GET", '/images/red.png');
    xhr.responseType = 'blob';
    xhr.send();
    xhr.onload = function() {
        resolve(xhr.response);
    };
});
var promise2 = new Promise(function(resolve, reject) {
    var xhr = new XMLHttpRequest();
    xhr.open("GET", '/images/green.png');
    xhr.responseType = 'blob';
    xhr.send();
    xhr.onload = function() {
        resolve(xhr.response);
    };
});
Promise.all([promise1, promise2]).then(function(response1, response2) {
    var promise3 = createImageBitmap(response1);
    var promise4 = createImageBitmap(response2);
    Promise.all([promise3, promise4]).then(function(bitmap1, bitmap2) {
        ctx.drawImage(bitmap2, 0, 0, 100, 50, 0, 0, 100, 50);
        ctx.drawImage(bitmap1, 0, 0, 100, 50, -100, 0, 100, 50);
        ctx.drawImage(bitmap1, 0, 0, 100, 50, 100, 0, 100, 50);
        ctx.drawImage(bitmap1, 0, 0, 100, 50, 0, -50, 100, 50);
        ctx.drawImage(bitmap1, 0, 0, 100, 50, 0, 50, 100, 50);
        _assertPixelApprox(canvas, 0,0, 0,255,0,255, 2);
        _assertPixelApprox(canvas, 99,0, 0,255,0,255, 2);
        _assertPixelApprox(canvas, 0,49, 0,255,0,255, 2);
        _assertPixelApprox(canvas, 99,49, 0,255,0,255, 2);
    }, t_fail);
}).then(t_pass, t_fail);

});
done();
