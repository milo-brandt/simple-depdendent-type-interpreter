// CodeMirror, copyright (c) by Marijn Haverbeke and others
// Hacked from the shell mode (by Milo Brandt)
// Distributed under an MIT license: https://codemirror.net/LICENSE

(function(mod) {
  if (typeof exports == "object" && typeof module == "object") // CommonJS
    mod(require("../../lib/codemirror"));
  else if (typeof define == "function" && define.amd) // AMD
    define(["../../lib/codemirror"], mod);
  else // Plain browser env
    mod(CodeMirror);
})(function(CodeMirror) {
"use strict";

CodeMirror.defineMode('dependenttt', function() {

  var words = {};
  function define(style, dict) {
    for(var i = 0; i < dict.length; i++) {
      words[dict[i]] = style;
    }
  };

  //var commonAtoms = ["true", "false"];
  var commonKeywords = ["block", "rule", "let", "axiom", "declare"];
  /*var commonCommands = ["ab", "awk", "bash", "beep", "cat", "cc", "cd", "chown", "chmod", "chroot", "clear",
    "cp", "curl", "cut", "diff", "echo", "find", "gawk", "gcc", "get", "git", "grep", "hg", "kill", "killall",
    "ln", "ls", "make", "mkdir", "openssl", "mv", "nc", "nl", "node", "npm", "ping", "ps", "restart", "rm",
    "rmdir", "sed", "service", "sh", "shopt", "shred", "source", "sort", "sleep", "ssh", "start", "stop",
    "su", "sudo", "svn", "tee", "telnet", "top", "touch", "vi", "vim", "wall", "wc", "wget", "who", "write",
    "yes", "zsh"];*/

  //CodeMirror.registerHelper("hintWords", "shell", commonAtoms.concat(commonKeywords, commonCommands));

  //define('atom', commonAtoms);
  define('keyword', commonKeywords);
  //define('builtin', commonCommands);

  function eatString(stream) {
    while(true) {
      var ch = stream.next()
      if(ch == '\\') {
        steam.next(); //ignore next character
      } else if(ch == '\"' || ch == null) {
        return;
      }
    }
  }
  function tokenBase(stream, state) {
    if (stream.eatSpace()) return null;

    var ch = stream.next();

    if (ch === '"') {
      eatString(stream);
      return "string";
    }
    if (ch === '#') {
      stream.skipToEnd();
      return 'comment';
    }
    if (/\d/.test(ch)) {
      stream.eatWhile(/\d/);
      return 'number';
    }
    if (/\w/.test(ch)) {
      stream.eatWhile(/\w/);
      var cur = stream.current();
      return words.hasOwnProperty(cur) ? words[cur] : null;
    }
    return null;
  }
  function tokenize(stream, state) {
    return tokenBase (stream, state);
  };
  return {
    startState: function() {return {};},
    token: function(stream, state) {
      return tokenize(stream, state);
    },
    closeBrackets: "()[]{}\"\"",
    lineComment: '#',
    fold: "brace"
  };
});
});
