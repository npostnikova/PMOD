from random import choices, sample
from copy import deepcopy

from evol_minimization.unknown_amq_params import unknown_params


class Individual:
    def __init__(self, params_vals):
        self.params_vals = params_vals

    def __str__(self):
        return "Individual {\n" + \
               ''.join([f'\t{key} : {value}\n' for key, value in
                        self.params_vals.items()]) + \
               "}\n"

    def __eq__(self, other):
        if not (other is Individual):
            return False
        return self.params_vals == other.params_vals

    def __hash__(self):
        return hash(frozenset(self.params_vals.items()))

    def create_mutation(self, affected_params, beta=0.7):
        result = deepcopy(self)
        for param in affected_params:
            cur_val = self.params_vals[param.name]
            possible_vals = list(range(param.min_val, param.max_val + 1))
            possible_vals.remove(cur_val)
            new_value = choices(
                possible_vals,
                k=1,
                weights=[abs(x - cur_val) ** (-beta) for x in possible_vals]
            )[0]
            result.params_vals[param.name] = new_value
        return result


def mutate(individual, beta=0.5):
    """Creates a mutation of the provided individual"""
    params_number = len(unknown_params)
    possible_params_number = range(1, params_number + 1)
    affected_params_cnt = choices(
        possible_params_number,
        k=1,
        weights=[x ** (-beta) for x in possible_params_number]
    )[0]
    print(affected_params_cnt)
    affected_params = sample(unknown_params, k=affected_params_cnt)
    print([x.name for x in affected_params])
    mutation = individual.create_mutation(affected_params)
    print(mutation)
    return mutation


def next_generation(
        prev_generation,
        tabu=frozenset(),
        children_num=1,
        attempts_num=10
):
    """Generates individuals using mutations.

    :param prev_generation: individuals to run mutations on
    :param tabu: individuals which cannot present in the new generation
    :param children_num: desired number of unique children from an individual
    :param attempts_num: num of attempts an individual has for unique mutations
    :return: list of individuals
    """
    generation = set()
    for individual in prev_generation:
        cnt = 0
        for _ in range(attempts_num):
            child = mutate(individual)
            if not (child in tabu) and not (child in generation):
                generation.add(child)
                cnt += 1
                if cnt >= children_num:
                    break
    return list(generation)
