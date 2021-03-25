use lsp_types::Diagnostic;
use std::sync::mpsc::Receiver;
use std::sync::mpsc::Sender;
use std::sync::mpsc::channel;


pub struct DiagnosticLog {
  recv:Receiver<Diagnostic>,
  send:Sender<Diagnostic>
}

impl DiagnosticLog {
  pub fn new() -> Self {
      let (send, recv) = channel();
      DiagnosticLog { recv, send }
  }

  pub fn getLogger(&mut self) -> DiagnosticLogger {
    DiagnosticLogger { sender: self.send.clone() }
  }
}


pub struct DiagnosticLogger {
  sender:Sender<Diagnostic>
}
impl DiagnosticLogger {

  pub fn logUnexpectedEOFInString() {
  }

  pub fn logInvalidControlChar(c:u8) {
  }

  pub fn logInvalidUnicodeCodePoint() {
  }

  pub fn logDigitExceedsRadix (radix:u8, digit: u8) {
  }

  pub fn logUnrecognizedRadixCode(code:u8) {
  }

  pub fn logUnrecognizedCharacter(character:u8) {
  }

  fn log(&mut self, d:Diagnostic) {
     self.sender.send(d).unwrap()
  }
}

