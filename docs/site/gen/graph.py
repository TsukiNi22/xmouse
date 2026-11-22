"""The interactive project graph (docs/graph.html, made by html-doc next to the site): found by the search like a page."""

def pages(ctx: object) -> list[dict]:
    return [] # the graph is its own page, not a Markdown one

def search_entries(ctx: object) -> list[dict]:
    texts = {
        "en": ("Project graph", "The files, classes and calls of xmouse, interactive (2D / 3D).", "graph project files classes functions calls sources"),
        "fr": ("Graphe du projet", "Les fichiers, classes et appels de xmouse, interactif (2D / 3D).", "graphe projet fichiers classes fonctions appels sources"),
    }
    return [{"k": "page", "l": lang, "t": texts[lang][0], "d": texts[lang][1], "u": "graph.html", "x": texts[lang][2]} for lang in ctx.langs if lang in texts]
