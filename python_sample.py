import matplotlib.pyplot as plt
import seaborn as sns

fig, axn = plt.subplots(2, 2, sharey='row')
hm = [[0, 1], [2, 3]]
hm2 = [[3, 5], [0, 7]]
sns.heatmap(hm, ax=axn[0][0])
sns.heatmap(hm, ax=axn[1][0])
sns.heatmap(hm2, ax=axn[0][1])
sns.heatmap(hm2, ax=axn[1][1])
plt.show()

# Checking that format works.
test_var = "OK"
print(f"Everything seems to be {test_var}")