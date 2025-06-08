import "gh:4lb0/emoji/emoji" for Emoji // Using main branch
import "gh:4lb0/emoji/export@1.0" for Emoji as EmojiV1 // Using 1.0 branch
if (EmojiV1.star == Emoji.star) {
  Response.out(Emoji.heart)
}
