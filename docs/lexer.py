"""
    pygments.lexers.wren
    ~~~~~~~~~~~~~~~~~~~~

    Lexer for Wren.

    :copyright: Copyright 2006-2024 by the Pygments team, see AUTHORS.
    :license: BSD, see LICENSE for details.
"""

import re

from pygments.lexer import include, RegexLexer, words
from pygments.token import Whitespace, Punctuation, Keyword, Name, Comment, \
    Operator, Number, String, Other

__all__ = ['BialetLexer']

class BialetLexer(RegexLexer):
    """
    For Wren source code, version 0.4.0.
    """
    name = 'Wren'
    url = 'https://bialet.dev'
    aliases = ['bialet']
    filenames = ['*.wren']
    version_added = '2.14'

    flags = re.MULTILINE | re.DOTALL

    tokens = {
        'root': [
            # Whitespace.
            (r'\s+', Whitespace),

            # HTML
            (r'\{\{', String.Interpol, 'html_interpolation'),
            (r'<\/?[a-z][a-z0-9]*[^>]*>', Other, 'html'),
            (r'<!doctype html>', Other),

            (r'[,\\\[\]{}]', Punctuation),

            # Really 'root', not '#push': in 'interpolation',
            # parentheses inside the interpolation expression are
            # Punctuation, not String.Interpol.
            (r'\(', Punctuation, 'root'),
            (r'\)', Punctuation, '#pop'),

            # Keywords.
            (words((
                'as', 'break', 'class', 'construct', 'continue', 'else',
                'for', 'foreign', 'if', 'import', 'return', 'static', 'super',
                'this', 'var', 'while'), prefix = r'(?<!\.)',
                suffix = r'\b'), Keyword),

            (words((
                'true', 'false', 'null'), prefix = r'(?<!\.)',
                suffix = r'\b'), Keyword.Constant),

            (words((
                'in', 'is'), prefix = r'(?<!\.)',
                suffix = r'\b'), Operator.Word),

            # Comments.
            (r'/\*', Comment.Multiline, 'comment'), # Multiline, can nest.
            (r'//.*?$', Comment.Single),            # Single line.
            (r'#.*?(\(.*?\))?$', Comment.Special),  # Attribute or shebang.

            # Names and operators.
            (r'[!%&*+\-./:<=>?\\^|~]+', Operator),
            (r'[a-z][a-zA-Z_0-9]*', Name),
            (r'[A-Z][a-zA-Z_0-9]*', Name.Class),
            (r'__[a-zA-Z_0-9]*', Name.Variable.Class),
            (r'_[a-zA-Z_0-9]*', Name.Variable.Instance),

            # Numbers.
            (r'0x[0-9a-fA-F]+', Number.Hex),
            (r'\d+(\.\d+)?([eE][-+]?\d+)?', Number.Float),

            # Strings.
            (r'""".*?"""', String),   # Raw string
            (r'"', String.Double, 'string_double'), # Other string
            (r"'", String.Single, 'string_single'), # Single quote string

            # Query
            (r'`([^`\\]*(?:\\.[^`\\]*)*)`', String.Backtick),

        ],
        'comment': [
            (r'/\*', Comment.Multiline, '#push'),
            (r'\*/', Comment.Multiline, '#pop'),
            (r'([^*/]|\*(?!/)|/(?!\*))+', Comment.Multiline),
        ],
        'string_double': [
            (r'"', String.Double, '#pop'),
            (r'\\[\\%"0abefnrtv]', String.Escape), # Escape.
            (r'\\x[a-fA-F0-9]{2}', String.Escape), # Byte escape.
            (r'\\u[a-fA-F0-9]{4}', String.Escape), # Unicode escape.
            (r'\\U[a-fA-F0-9]{8}', String.Escape), # Long Unicode escape.

            (r'%\(', String.Interpol, 'interpolation'),
            (r'[^\\"%]+', String.Double), # All remaining characters.
        ],
        'string_single': [
            (r"'", String.Single, '#pop'),
            (r'\\[\\%"0abefnrtv]', String.Escape), # Escape.
            (r'\\x[a-fA-F0-9]{2}', String.Escape), # Byte escape.
            (r'\\u[a-fA-F0-9]{4}', String.Escape), # Unicode escape.
            (r'\\U[a-fA-F0-9]{8}', String.Escape), # Long Unicode escape.

            (r'%\(', String.Interpol, 'interpolation'),
            (r"[^\\'%]+", String.Single), # All remaining characters.
        ],
        'html': [
            (r'<[^>]+>', Other, '#pop'),
            (r'\{\{', String.Interpol, 'html_interpolation'),
            (r'[^\\<]+', Other),
        ],
        'interpolation': [
            # redefine closing paren to be String.Interpol
            (r'\)', String.Interpol, '#pop'),
            include('root'),
        ],
        'html_interpolation': [
            (r'\}\}', String.Interpol, '#pop'),
            include('root'),
        ],
    }
