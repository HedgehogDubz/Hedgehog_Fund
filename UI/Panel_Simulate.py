from tabdock import Panel


class Simulate(Panel):
    def __init__(self, parent, docked, x, y, w, h, **kw):
        super().__init__(parent, docked, x, y, w, h, **kw)

        self.add_dropdown(
            ["Random Walk", "Strategy 2", "Strategy 3"],
            string_key="simulate_strategy",
            default="Random Walk",
        )
        
        
        