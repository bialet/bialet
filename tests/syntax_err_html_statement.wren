// A bare HTML literal as a statement: its value is discarded, so a method
// that expects it as the result returns null. The diagnostic should name the
// literal's line instead of the method's closing brace and EOF.
class Card {
  static render(value) {
    if (value > 6) return <span class="big">{{ value }}</span>
    <span class="small">{{ value }}</span>
  }
}
