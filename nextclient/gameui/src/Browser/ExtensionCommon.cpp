#include "ExtensionCommon.h"
#include <GameUi.h>
#include <TaskRun.h>

#include "AcceptedDomains.h"

bool CExtensionCommonHandler::Execute(
	const CefString& name, CefRefPtr<CefV8Value> object,
	const CefV8ValueList& arguments, CefRefPtr<CefV8Value>& retval, CefString& exception
) {
	if(!IsV8CurrentContextOnAcceptedDomain()) return false;

	if(name == "exec" && !arguments.empty()) {
		const auto cmd = std::string(arguments[0]->GetStringValue());
		TaskCoro::RunInMainThread([this, cmd] {
			engine_->pfnClientCmd(cmd.c_str());
		});

		return true;
	}

	return false;
}

void RegisterCommonJsApi(cl_enginefunc_t* engine) {
	CefString code = 
		"function makeAsEventTarget(object) {"
		"	object.__defineGetter__('eventTarget', function() {"
		"		if(!this._eventTarget) this._eventTarget = document.createDocumentFragment();"
		"		return this._eventTarget;"
		"	});"
		"	object.addEventListener = function() {"
		"		return object.eventTarget.addEventListener.apply(object.eventTarget, arguments);"
		"	};"
		"	object.removeEventListener = function() {"
		"		return object.eventTarget.removeEventListener.apply(object.eventTarget, arguments);"
		"	};"
		"	object.dispatchEvent = function(type, params) {"
		"		return object.eventTarget.dispatchEvent(new CustomEvent(type, { detail: params, cancelable: true }));"
		"	};"
		"};"
		"var nextclient = {};"
		"(function() {"
		"	nextclient.exec = function(cmd) {"
		"		native function exec();"
		"		if(typeof cmd == 'string')"
		"			return exec(cmd);"
		"	};"
		"	makeAsEventTarget(nextclient);"
		"})();";
		"!function(e,t){'object'==typeof exports&&'undefined'!=typeof module?t():'function'==typeof define&&define.amd?define(t):t()}(0,function(){'use strict';function e(e){var t=this.constructor;return this.then(function(n){return t.resolve(e()).then(function(){return n})},function(n){return t.resolve(e()).then(function(){return t.reject(n)})})}function t(e){return new this(function(t,n){function r(e,n){if(n&&('object'==typeof n||'function'==typeof n)){var f=n.then;if('function'==typeof f)return void f.call(n,function(t){r(e,t)},function(n){o[e]={status:'rejected',reason:n},0==--i&&t(o)})}o[e]={status:'fulfilled',value:n},0==--i&&t(o)}if(!e||'undefined'==typeof e.length)return n(new TypeError(typeof e+' '+e+' is not iterable(cannot read property Symbol(Symbol.iterator))'));var o=Array.prototype.slice.call(e);if(0===o.length)return t([]);for(var i=o.length,f=0;o.length>f;f++)r(f,o[f])})}function n(e,t){this.name='AggregateError',this.errors=e,this.message=t||''}function r(e){var t=this;return new t(function(r,o){if(!e||'undefined'==typeof e.length)return o(new TypeError('Promise.any accepts an array'));var i=Array.prototype.slice.call(e);if(0===i.length)return o();for(var f=[],u=0;i.length>u;u++)try{t.resolve(i[u]).then(r)['catch'](function(e){f.push(e),f.length===i.length&&o(new n(f,'All promises were rejected'))})}catch(c){o(c)}})}function o(e){return!(!e||'undefined'==typeof e.length)}function i(){}function f(e){if(!(this instanceof f))throw new TypeError('Promises must be constructed via new');if('function'!=typeof e)throw new TypeError('not a function');this._state=0,this._handled=!1,this._value=undefined,this._deferreds=[],s(e,this)}function u(e,t){for(;3===e._state;)e=e._value;0!==e._state?(e._handled=!0,f._immediateFn(function(){var n=1===e._state?t.onFulfilled:t.onRejected;if(null!==n){var r;try{r=n(e._value)}catch(o){return void a(t.promise,o)}c(t.promise,r)}else(1===e._state?c:a)(t.promise,e._value)})):e._deferreds.push(t)}function c(e,t){try{if(t===e)throw new TypeError('A promise cannot be resolved with itself.');if(t&&('object'==typeof t||'function'==typeof t)){var n=t.then;if(t instanceof f)return e._state=3,e._value=t,void l(e);if('function'==typeof n)return void s(function(e,t){return function(){e.apply(t,arguments)}}(n,t),e)}e._state=1,e._value=t,l(e)}catch(r){a(e,r)}}function a(e,t){e._state=2,e._value=t,l(e)}function l(e){2===e._state&&0===e._deferreds.length&&f._immediateFn(function(){e._handled||f._unhandledRejectionFn(e._value)});for(var t=0,n=e._deferreds.length;n>t;t++)u(e,e._deferreds[t]);e._deferreds=null}function s(e,t){var n=!1;try{e(function(e){n||(n=!0,c(t,e))},function(e){n||(n=!0,a(t,e))})}catch(r){if(n)return;n=!0,a(t,r)}}n.prototype=Error.prototype;var d=setTimeout;f.prototype['catch']=function(e){return this.then(null,e)},f.prototype.then=function(e,t){var n=new this.constructor(i);return u(this,new function(e,t,n){this.onFulfilled='function'==typeof e?e:null,this.onRejected='function'==typeof t?t:null,this.promise=n}(e,t,n)),n},f.prototype['finally']=e,f.all=function(e){return new f(function(t,n){function r(e,o){try{if(o&&('object'==typeof o||'function'==typeof o)){var u=o.then;if('function'==typeof u)return void u.call(o,function(t){r(e,t)},n)}i[e]=o,0==--f&&t(i)}catch(c){n(c)}}if(!o(e))return n(new TypeError('Promise.all accepts an array'));var i=Array.prototype.slice.call(e);if(0===i.length)return t([]);for(var f=i.length,u=0;i.length>u;u++)r(u,i[u])})},f.any=r,f.allSettled=t,f.resolve=function(e){return e&&'object'==typeof e&&e.constructor===f?e:new f(function(t){t(e)})},f.reject=function(e){return new f(function(t,n){n(e)})},f.race=function(e){return new f(function(t,n){if(!o(e))return n(new TypeError('Promise.race accepts an array'));for(var r=0,i=e.length;i>r;r++)f.resolve(e[r]).then(t,n)})},f._immediateFn='function'==typeof setImmediate&&function(e){setImmediate(e)}||function(e){d(e,0)},f._unhandledRejectionFn=function(e){void 0!==console&&console&&console.warn('Possible Unhandled Promise Rejection:',e)};var p=function(){if('undefined'!=typeof self)return self;if('undefined'!=typeof window)return window;if('undefined'!=typeof global)return global;throw Error('unable to locate global object')}();'function'!=typeof p.Promise?p.Promise=f:(p.Promise.prototype['finally']||(p.Promise.prototype['finally']=e),p.Promise.allSettled||(p.Promise.allSettled=t),p.Promise.any||(p.Promise.any=r))});";

	CefRegisterExtension("ncl/common", code, new CExtensionCommonHandler(engine));
}