//
//  This file is part of Bialet, which is licensed under the
//  GNU General Public License, version 2 (GPL-2.0).
//
//  Copyright (c) 2023 Rodrigo Arce
//
//  SPDX-License-Identifier: GPL-2.0-only
//
//  For full license text, see LICENSE.md.
//

import "bialet" for Request, Session, Response

class Resource {
}

class Auth {
  static init() {
    __user = false
    __denied = false
  }
  static denied { __denied }
  static denied=(d) { __denied = d }
  static check {
    if (!__denied) {
      __user ? deny() : require()
      return true
    }
    return false
  }
  static require() { Response.login() }
  static deny() { Response.forbidden() }
  static user { __user }
}

class User {
  construct new() {
    _name = ""
    _isAdmin = false
  }
  isAdmin { _isAdmin }
}

Auth.init()
