#!/usr/bin/env python

import pandas as pd

# Load the CSV files into pandas DataFrames
df1 = pd.read_csv('data_1.csv', sep=' ')
df2 = pd.read_csv('data_2.csv', sep=' ')

# Merge the DataFrames based on a common column
merged_df = pd.merge(df1, df2, on='util')

# Save the merged DataFrame to a new CSV file
merged_df.to_csv('data_merged.csv', index=False, sep=' ')
