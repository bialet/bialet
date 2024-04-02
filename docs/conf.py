# Configuration file for the Sphinx documentation builder.
#
# For the full list of built-in configuration values, see the documentation:
# https://www.sphinx-doc.org/en/master/usage/configuration.html

# -- Project information -----------------------------------------------------
# https://www.sphinx-doc.org/en/master/usage/configuration.html#project-information

project = 'Bialet'
copyright = '2024, Rodrigo Arce'
author = 'Rodrigo Arce'
version = '0.4'

# -- General configuration ---------------------------------------------------
# https://www.sphinx-doc.org/en/master/usage/configuration.html#general-configuration

extensions = ['myst_parser', 'sphinx_design']

templates_path = ['_templates']
exclude_patterns = ['requirements.txt']

highlight_language = 'wren'
highlight_options = {'guess_lang': False}



# -- Options for HTML output -------------------------------------------------
# https://www.sphinx-doc.org/en/master/usage/configuration.html#options-for-html-output

html_theme = 'sphinx_book_theme'
html_static_path = ['_static']
html_favicon = '../src/favicon.ico'
html_theme_options = {
    "logo": {
        "text": "Bialet documentation",
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

source_suffix = {
    '.rst': 'restructuredtext',
    '.txt': 'markdown',
    '.md': 'markdown',
}
