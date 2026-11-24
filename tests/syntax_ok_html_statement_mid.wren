// A bare HTML literal between two statements. The lexer keeps the newline
// here, so the statement ends normally; the block loop must not call
// consumeLine again and eat the next statement.
class Card {
  static render(value) {
    var title = "x"
    <span>{{ value }}</span>
    return title
  }
}
