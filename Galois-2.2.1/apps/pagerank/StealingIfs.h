#ifdef STEALING
if (wl == "smq_1_4")
  Galois::for_each(boost::make_transform_iterator(graph.begin(), std::ref(fn)),
             boost::make_transform_iterator(graph.end(), std::ref(fn)),
             Process(graph, tolerance, amp), Galois::wl<SMQ_1_4>());
if (wl == "smq_1_8")
  Galois::for_each(boost::make_transform_iterator(graph.begin(), std::ref(fn)),
             boost::make_transform_iterator(graph.end(), std::ref(fn)),
             Process(graph, tolerance, amp), Galois::wl<SMQ_1_8>());
if (wl == "smq_1_16")
  Galois::for_each(boost::make_transform_iterator(graph.begin(), std::ref(fn)),
             boost::make_transform_iterator(graph.end(), std::ref(fn)),
             Process(graph, tolerance, amp), Galois::wl<SMQ_1_16>());
if (wl == "smq_1_32")
  Galois::for_each(boost::make_transform_iterator(graph.begin(), std::ref(fn)),
             boost::make_transform_iterator(graph.end(), std::ref(fn)),
             Process(graph, tolerance, amp), Galois::wl<SMQ_1_32>());
if (wl == "smq_1_64")
  Galois::for_each(boost::make_transform_iterator(graph.begin(), std::ref(fn)),
             boost::make_transform_iterator(graph.end(), std::ref(fn)),
             Process(graph, tolerance, amp), Galois::wl<SMQ_1_64>());
if (wl == "smq_1_128")
  Galois::for_each(boost::make_transform_iterator(graph.begin(), std::ref(fn)),
             boost::make_transform_iterator(graph.end(), std::ref(fn)),
             Process(graph, tolerance, amp), Galois::wl<SMQ_1_128>());
if (wl == "smq_1_256")
  Galois::for_each(boost::make_transform_iterator(graph.begin(), std::ref(fn)),
             boost::make_transform_iterator(graph.end(), std::ref(fn)),
             Process(graph, tolerance, amp), Galois::wl<SMQ_1_256>());
#endif
