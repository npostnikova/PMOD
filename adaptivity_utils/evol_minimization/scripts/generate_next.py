import csv
import argparse
from pathlib import Path
from random import randint

from evol_minimization.mutation import next_generation, Individual
from evol_minimization.unknown_amq_params import unknown_params


def save_generation(generation):
    with open("next_generation.csv", "w", newline="") as csvfile:
        writer = csv.DictWriter(csvfile,
                                fieldnames=[x.name for x in unknown_params])
        writer.writeheader()
        for individual in generation:
            writer.writerow(individual.params_vals)


def read_prev_generation(path="active_ancestors.csv"):
    ancestors = []
    if not Path(path).exists():
        return ancestors
    with open(path, "r", newline="") as csvfile:
        reader = csv.DictReader(csvfile)
        for row in reader:
            params = {x.name: int(row[x.name]) for x in unknown_params}
            ancestors.append(Individual(params))
    return ancestors


# All produced individuals
def read_all_individuals(path="all_individuals.csv"):
    all_individuals = set()
    if not Path(path).exists():
        return all_individuals
    with open(path, "r", newline="") as csvfile:
        reader = csv.DictReader(csvfile)
        for row in reader:
            params = {x.name: int(row[x.name]) for x in unknown_params}
            all_individuals.add(Individual(params))
    return all_individuals


# todo which size
def create_random_generation(gen_size=100):
    if gen_size is None:
        gen_size = 100
    gen = set()
    while len(gen) < gen_size:
        individual = Individual(
            {param.name: randint(param.min_val, param.max_val) for param in
             unknown_params}
        )
        gen.add(individual)
    return list(gen)


def build_next_generation(gen_size=None):
    ancestors = read_prev_generation()
    if len(ancestors) == 0:
        gen = create_random_generation(gen_size)
    else:
        tabu = read_all_individuals()
        gen = next_generation(ancestors, tabu=tabu, children_num=3)
    save_generation(gen)
    # append_individuals(gen) todo: after time?


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument('--gen_size', type=int,
                        help='Size of the generation', default=None)
    args = parser.parse_args()
    build_next_generation(args.gen_size)
