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
    Param("stat_buff_size", 1,  10, lambda x: 2 ** x),
    Param("stat_prob_size", 1,  10, lambda x: 2 ** x),
    Param("inc_size_per", 5,  19, lambda x: 5 * x),
    Param("dec_size_per", 1,  15, lambda x: 5 * x),
    Param("inc_prob_per", 5,  19, lambda x: 5 * x),
    Param("dec_prob_per", 1,  15, lambda x: 5 * x),
)
