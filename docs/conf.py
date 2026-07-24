# Configuration file for the Sphinx documentation builder.
#
# For the full list of built-in configuration values, see the documentation:
# https://www.sphinx-doc.org/en/master/usage/configuration.html

import os
import re

# -- Project information -----------------------------------------------------
# https://www.sphinx-doc.org/en/master/usage/configuration.html#project-information

project = 'Bialet'
copyright = '2026, Rodrigo Arce'
author = 'Rodrigo Arce'


def _get_bialet_version():
    header = os.path.join(os.path.dirname(__file__), '..', 'src', 'bialet.h')
    with open(header) as f:
        for line in f:
            m = re.match(r'#define\s+BIALET_VERSION\s+"([^"]+)"', line)
            if m:
                return m.group(1)
    return '0.0.0'


_bialet_version = _get_bialet_version()
version = _bialet_version
_release_tag = f'v{_bialet_version}'
_release_base = f'https://github.com/bialet/bialet/releases/download/{_release_tag}'
_release_urls = {
    'linux_arm64': f'{_release_base}/bialet-{_release_tag}-linux-arm64.tar.gz',
    'linux_x64': f'{_release_base}/bialet-{_release_tag}-linux-x86_64.tar.gz',
    'macos_arm64': f'{_release_base}/bialet-{_release_tag}-macos-arm64.tar.gz',
    'windows': f'{_release_base}/bialet-{_release_tag}-windows-x86_64.zip',
}

_substitutions = {
    'bialet_version': _bialet_version,
    'release_tag': _release_tag,
    'release_base_url': _release_base,
    'release_linux_arm64_url': _release_urls['linux_arm64'],
    'release_linux_x64_url': _release_urls['linux_x64'],
    'release_macos_arm64_url': _release_urls['macos_arm64'],
    'release_windows_url': _release_urls['windows'],
}


def _replace_placeholders(app, docname, source):
    for key, value in _substitutions.items():
        source[0] = source[0].replace('{{' + key + '}}', value)


def setup(app):
    app.connect('source-read', _replace_placeholders)

# -- General configuration ---------------------------------------------------
# https://www.sphinx-doc.org/en/master/usage/configuration.html#general-configuration

extensions = ['myst_parser', 'sphinx_design', 'sphinxext.opengraph', 'sphinx_copybutton']

templates_path = ['_templates']
exclude_patterns = ['requirements.txt', 'BIALET_PROMPT.md', 'examples/example.md']

highlight_language = 'wren'
highlight_options = {'guess_lang': False}

suppress_warnings = ['misc.highlighting_failure']


# -- Options for HTML output -------------------------------------------------
# https://www.sphinx-doc.org/en/master/usage/configuration.html#options-for-html-output

html_theme = 'sphinx_book_theme'
html_static_path = ['_static']
html_css_files = ["custom.css"]
html_favicon = '../src/favicon.ico'
html_theme_options = {
    "logo": {
        "text": "Bialet Documentation",
        "image_light": "_static/logo.png",
        "image_dark": "_static/logo.png",
    },
    "repository_url": "https://github.com/bialet/bialet",
    "path_to_docs": "docs",
    "use_edit_page_button": True,
    "use_repository_button": True,
    "use_issues_button": True,
}
html_show_copyright = False
html_show_sphinx = False

ogp_site_url = "https://bialet.dev/"
ogp_image = "https://bialet.dev/_static/og-image.png"

source_suffix = {
    '.rst': 'restructuredtext',
    '.txt': 'markdown',
    '.md': 'markdown',
}
