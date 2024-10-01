import "bialet" for Response
import <4lb0/emoji> for Emoji // Using main branch
import <4lb0/emoji@1.0> for Emoji as EmojiV1 // Using 1.0 branch
if (EmojiV1.star == Emoji.star) {
  Response.out(Emoji.heart)
}
