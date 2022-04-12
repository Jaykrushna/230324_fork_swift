// RUN: %target-typecheck-verify-swift -enable-experimental-string-processing -disable-implicit-string-processing-module-import
// REQUIRES: swift_in_compiler

// expected-error @+1 {{missing 'Regex' declaration, probably because the '_StringProcessing' module was not imported properly}}
let r0 = #/./#
// expected-error @+1 {{cannot find type 'Regex' in scope}}
let _: Regex<Substring> = r0