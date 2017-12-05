import sys, os, statistics

target_dir = sys.argv[1]

print("Max Drift,Rapport Period,Amortization Period,Avg. Error,Median Error,Max Absolute Error,Error Standard Deviation")
for filename in os.listdir(target_dir):
	if not filename.endswith(".txt"):
		continue

	basename = filename[0:len(filename) - 4]
	max_drift, rapport_period, amortization = map(int, basename.split('_'))
	with open(target_dir + filename) as f:
		error_list = []
		prelude = False
		rapport = 0
		for line in f:
			if not prelude:
				try:
					line.index("Current Real Time,Local Server Time,Hardware Clock Time,Software Clock Time,Error,Remote Est Time")
					prelude = True
				except ValueError:
					pass
				continue

			err, est = list(line.split(','))[-3:-1]
			err = int(err)
			if est != '':
				rapport += 1
			if rapport >= 2:
				error_list += [err]

		abs_error_list = list(map(abs, error_list))
		avg_error = statistics.mean(error_list)
		median_error = statistics.median(error_list)
		max_abs_error = max(abs_error_list)
		stdev_error = statistics.stdev(error_list)
		
		print("{0},{1},{2},{3},{4},{5},{6}".format(max_drift, rapport_period, amortization, avg_error, median_error, max_abs_error, stdev_error))
