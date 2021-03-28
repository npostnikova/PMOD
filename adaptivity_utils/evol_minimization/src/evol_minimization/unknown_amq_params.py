class Param:
    """Description of an AMQ parameter."""
    def __init__(self, name, min_val, max_val, get_val):
        """Arguments:
        'name'    -- the name of the param
        'min_val' -- minimum param's distribution value
        'max_val' -- maximum param's distribution value
        'get_val' -- maps distribution value to real params value
        """
        self.   name = name
        self.min_val = min_val
        self.max_val = max_val
        self.get_val = get_val


# List of parameters, which are considered for minimization.
unknown_params = (
    Param("pushQ",        1,  10, lambda x: 2 ** x),
    Param("popQ",         1,  10, lambda x: 2 ** x),
    Param("s",            1,  10, lambda x:      x),
    Param("f",            1,  10, lambda x:      x),
    Param("e",            1,  10, lambda x:      x),
    Param("segment_size", 1,  10, lambda x: 2 ** x),
    Param("percent",      12, 20, lambda x:  x * 5),
    Param("resume_size",  1,  10, lambda x: 2 ** x)
)
