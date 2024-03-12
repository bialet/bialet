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

import "bialet" for Request, Session, Response, Util, Db

class Resource {
}

class Auth {
  static init {
    __user = false
    __denied = false
    var authHeader = Request.header("authorization")
    if (authHeader) {
      var tmp = authHeader.split(" ")
      if (tmp[0] == "Basic" && tmp.count > 1) {
        var credentials = Util.base64Decode(tmp[1])
        System.print("Credentials: %(credentials)")
      }
    }
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
  static login(email, password) {
    var user = `SELECT * FROM BIALET_USERS WHERE email = ?`.first([email])
    if (user) {
      if (user["password"] == Util.hash(password)) {
        Session.set("BIALET_USER", __user.id)
        __user = User.new(user)
        return true
      }
    }
    return false
  }
  static require() { Response.login() }
  static deny() { Response.forbidden() }
  static user { __user }
}

class User {
  construct new() {
    _id = Session.new().get("BIALET_USER")
    _name = ""
    _email = ""
    _password = ""
    _isAdmin = false
    if (_id) {
      var res = `SELECT * FROM BIALET_USERS WHERE id = ?`.first([_id])
      if (res) {
        _name = res["name"]
        _email = res["email"]
        _password = res["password"]
        _isAdmin = res["isAdmin"] == "1"
      }
    }
  }
  construct new(data) {
    _id = data["id"]
    _name = data["name"]
    _email = data["email"]
    _password = data["password"]
    _isAdmin = data["isAdmin"] == "1"
  }
  id { _id }
  id=(id) { _id = id }
  name { _name }
  name=(n) { _name = n }
  email { _email }
  email=(e) { _email = e }
  password { _password }
  password=(p) { _password = Util.hash(p) }
  isAdmin { _isAdmin }
  isAdmin=(a) { _isAdmin = a }
  save() {
    _id = Db.save("BIALET_USERS", {"name": _name, "email": _email, "password": _password, "isAdmin": _isAdmin, "id": _id, "updatedAt": `CURRENT_TIMESTAMP`})
    Session.new().set("BIALET_USER", _id)
  }
}

Auth.init
