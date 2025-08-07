import pandas as pd
import matplotlib.pyplot as plt
import matplotlib.dates as mdates

# === Load datasets ===
df1 = pd.read_csv("/home/alessandro/zalf-rpm/monica/output_standard.csv", skiprows=[0, 2], header=0)
df1 = df1.iloc[:2540]
df1['Date'] = pd.to_datetime(df1['Date'], format='%Y-%m-%d', errors='coerce')
df1 = df1.dropna(subset=['Date'])

df2 = pd.read_csv("/home/alessandro/zalf-rpm/monica/sim-min-out.csv", skiprows=[0, 2], header=0)
df2 = df2.iloc[:2540]
df2['Date'] = pd.to_datetime(df2['Date'], format='%Y-%m-%d', errors='coerce')
df2 = df2.dropna(subset=['Date'])

# === Filter date window ===
start_date = "1991-08-01"
end_date = "1997-08-31"
df1 = df1[(df1['Date'] >= start_date) & (df1['Date'] <= end_date)]
df2 = df2[(df2['Date'] >= start_date) & (df2['Date'] <= end_date)]

# Columns that SHOULD be numeric (adjust as needed)
numeric_cols_df1 = ['Yield', 'Evapotranspiration', 'AbBiom', 'N', 'Mois']
numeric_cols_df2 = ['Yield1', 'Evapotranspiration1', 'AbBiom1', 'Yield2', 'Evapotranspiration2', 'AbBiom2', 'Mois', 'N']

# Convert to numeric in df1
for col in numeric_cols_df1:
    df1[col] = pd.to_numeric(df1[col], errors='coerce')

# Convert to numeric in df2
for col in numeric_cols_df2:
    df2[col] = pd.to_numeric(df2[col], errors='coerce')

# === Define variables to plot ===
variables = ['Stage', 'Evapotranspiration', 'AbBiom', 'Yield', 'LAI', 'Height', 'EffRootDep', 'N', 'Mois']

# === Styling ===
dataset_colors = {
    'baseline': 'red',
    'intercropping': 'green'
}

scalar_linestyles = {
    'baseline': 'solid',
    'C1': 'dashed',
    'C2': 'dotted'
}

# === Plotting ===
n_rows = 3
n_cols = 3
fig, axes = plt.subplots(n_rows, n_cols, figsize=(15, 12), sharex=True)
axes = axes.flatten()

for i, var in enumerate(variables):
    if i >= len(axes):
        break

    ax = axes[i]

    # --- Plot baseline data (df1) ---
    if var in df1.columns:
        ax.plot(df1['Date'], df1[var],
                color=dataset_colors['baseline'],
                linestyle=scalar_linestyles['baseline'],
                label=f'{var} (baseline)')

    # --- Plot intercropping data (df2) ---
    # Special handling for Mois and N which don't have suffixes in df2
    if var in ['Mois', 'N']:
        if var in df2.columns:
            ax.plot(df2['Date'], df2[var],
                    color=dataset_colors['intercropping'],
                    linestyle=scalar_linestyles['C1'],
                    label=f'{var} (intercropping)')
    else:
        # Normal handling for other variables with suffixes
        if f"{var}1" in df2.columns:
            ax.plot(df2['Date'], df2[f"{var}1"],
                    color=dataset_colors['intercropping'],
                    linestyle=scalar_linestyles['C1'],
                    label=f'{var} (C1)')

        if f"{var}2" in df2.columns:
            ax.plot(df2['Date'], df2[f"{var}2"],
                    color=dataset_colors['intercropping'],
                    linestyle=scalar_linestyles['C2'],
                    label=f'{var} (C2)')

    ax.set_title(var)
    ax.grid(True)
    ax.xaxis.set_major_formatter(mdates.DateFormatter('%b-%Y'))
    ax.tick_params(axis='x', rotation=45)

    # Clean duplicate legend entries
    handles, labels = ax.get_legend_handles_labels()
    by_label = dict(zip(labels, handles))
    ax.legend(by_label.values(), by_label.keys(), fontsize='small')

# Hide any unused subplots
for j in range(i+1, len(axes)):
    axes[j].axis('off')

# === Final layout ===
plt.suptitle("Comparison: Single Crop vs. Intercropping (Daily Values)", fontsize=16)
plt.tight_layout(rect=[0, 0, 1, 0.97])
plt.show()