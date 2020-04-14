# Plots for amq

import json
import matplotlib.ticker as tckr
import matplotlib.pyplot as plt
from matplotlib.patches import Patch
from matplotlib.lines import Line2D

class ExecutionRecord:
  def __init__(self, app, wl, graph, threads, time):
    self.app = app
    self.wl = wl
    self.graph = graph
    self.threads = threads
    self.time = time

colors = ['g', 'b', 'r', 'c', 'k', 'm']

def next_color():
  next_color.col = (next_color.col + 1) % len(colors)
  return colors[next_color.col]
next_color.col = 0


def extract_data(command):
  data = command.split()
  app = "unknown_app"
  wl = "obim"
  graph = "unknown_graph"
  for d in data:
    if "app" in d:
      app = d.split('/')[-1]
      continue
    if ".gr" in d or "input" in d:
      graph = d.split('/')[-1]
      continue
    if "-wl" in d:
      wl = d.split('=')[-1]
  return (app, wl, graph)

def make_plot(execs):
  # x
  gr_cnt = 0
  graphs = {}

  # y
  wl_cnt = 0
  wls = {}
  for e in execs:
    if e.graph not in graphs:
      graphs[e.graph] = gr_cnt
      gr_cnt += 1
    if e.wl not in wls:
      wls[e.wl] = wl_cnt
      wl_cnt += 1
  print graphs
  print wls
  fig, axs = plt.subplots(wl_cnt, gr_cnt) #, sharex='col', sharey='row',
  #gridspec_kw={'hspace': 0, 'wspace': 0})
  fig.suptitle(execs[0].app)
  if gr_cnt == 0 or wl_cnt == 0:
    return

  k = 0
  for e in execs:
    j = graphs[e.graph]
    i = wls[e.wl]
    xs = e.threads
    ys = e.time
    if gr_cnt == 1 and wl_cnt != 1:
      axs[k].plot(xs, ys, colors[wls[wl.e]])
      axs[k].set(xlabel=e.wl, ylabel=e.graph)
      k += 1
      continue
    if wl_cnt == 1:
      axs.plot(xs, ys, next_color())
      axs.set(xlabel=e.wl, ylabel=e.graph)
      continue
    axs[i, j].plot(xs, ys, colors[wls[wl.e]])
    axs[i, j].set(xlabel=e.graph, ylabel=e.wl)
  if wl_cnt > 1 or gr_cnt > 1:
    for ax in axs.flat:
      ax.label_outer()
  fig.savefig(execs[0].app, dpi=100)
  plt.show()
  #fig.savefig( 'fig.pdf', bbox_inches='tight')



def make_combined_plot(execs):
  wl_cnt = 0
  wls = {}

  gr_cnt = 0
  grs = {}
  for e in execs:
    if e.graph not in grs:
      grs[e.graph] = gr_cnt
      gr_cnt += 1
    if e.wl not in wls:
      wls[e.wl] = wl_cnt
      wl_cnt += 1
  print wls
  lines = []
  for wl in wls:
    print wl
    lines.append(Line2D([0],[0], color=colors[wls[wl] % len(colors)], lw=2))

  n = gr_cnt
  fig, axs = plt.subplots(1, n)
  #if n == 1:
  #axs.set_title(apps[0])
  #else:
  #for a in apps:
  #axs[apps[a]].set_title(a)

  #plt.subplots(wl_cnt, gr_cnt, sharex='col', sharey='row',
  #                     gridspec_kw={'hspace': 0, 'wspace': 0})
  fig.suptitle(execs[0].app)
  #fig.legend(lines, wls.keys(), bbox_to_anchor=(1.04,0.5), loc="center left", borderaxespad=0)#loc='center left', bbox_to_anchor=(1, 0.5))

  if n == 0:
    return

  k = 0
  ticks = []
  ticks_val = []
  for e in execs:
    col = colors[wls[e.wl] % len(colors)]
    xs = e.threads
    ys = e.time

    if n == 1:
      axs.plot(xs, map(lambda x: x / 1e5, ys), col) #next_color())
      #for i in range(len(ys)):

      axs.set(xlabel=e.graph, ylabel='')
      continue
    plot_id = grs[e.graph]
    axs[plot_id].plot(xs, map(lambda x: x / 1e6, ys), col)
    ticks = ticks + xs
    ticks_val = ticks_val + map(lambda x: x / 1e6, ys)
    axs[plot_id].set(xlabel=e.graph, ylabel='')
  #if wl_cnt > 1 or gr_cnt > 1:
  #for ax in axs.flat:
  #ax.label_outer()
  y_formatter = tckr.ScalarFormatter(useOffset=False)
  if n == 1:
    axs.yaxis.set_major_formatter(y_formatter)
  else:
    for i in range(0, n):
      axs[i].yaxis.set_major_formatter(y_formatter)
  plt.legend( lines, wls, bbox_to_anchor = (0,-0.1,1,1),loc='upper center',
              bbox_transform = plt.gcf().transFigure )
  ####plt.yticks([1, 100000, 200000] ,["m", "e", "ow"]) #ticks, ticks_val)
  fig.savefig(execs[0].app, dpi=200)
  plt.show()
  #fig.savefig( 'fig.pdf', bbox_inches='tight')



with open('./amq_reports/report.json', 'r') as f:
  report_dict = json.load(f)
  print report_dict.keys()


inds = [[0, 0], [0, 1], [1, 0], [1, 1]]
i = 0
apps = {} # i will learn python one day
#apps.remove("it is set")
execs = []
apps_cnt = 0
for value in report_dict.values():
  xs = value['data']['Threads']
  ys = value['data']['Time']
  (app, wl, graph) = extract_data(value['command'])
  if app not in apps:
    apps[app] = apps_cnt
    apps_cnt += 1

  i += 1
  execs.append(ExecutionRecord(app, wl, graph, xs, ys))
print apps
print execs
execs = filter(lambda e: e.threads[0] != 0, execs)
for app in apps:
  ex = filter(lambda e: e.app == app, execs)
  print "hell"
  print ex
  make_combined_plot(ex)

