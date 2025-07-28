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
end_date = "1992-08-31"
df1 = df1[(df1['Date'] >= start_date) & (df1['Date'] <= end_date)]
df2 = df2[(df2['Date'] >= start_date) & (df2['Date'] <= end_date)]

# Columns that SHOULD be numeric (adjust as needed)
numeric_cols_df1 = ['Yield', 'Evapotranspiration', 'AbBiom']
numeric_cols_df2 = ['Yield1', 'Evapotranspiration1', 'AbBiom1']

# Convert to numeric in df1
for col in numeric_cols_df1:
    df1[col] = pd.to_numeric(df1[col], errors='coerce')  # 'coerce' turns invalid values to NaN

# Convert to numeric in df2
for col in numeric_cols_df2:
    df2[col] = pd.to_numeric(df2[col], errors='coerce')

# === Define variable groups ===
scalar_vars = ['Stage', 'Evaporation', 'Evapotranspiration', 'Tra', 'Act_ET', 'AbBiom']
layered_var_groups = {
    'Mois': ['Mois_1', 'Mois_2', 'Mois_3'],
    'N': ['N_1', 'N_2', 'N_3'],
    'rootDensity': ['rootDensity_1', 'rootDensity_2', 'rootDensity_3']
}
variables = scalar_vars + list(layered_var_groups.keys())  # 6 scalars + 3 grouped = 9 plots

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

layer_styles = {
    1: {'linestyle': 'solid', 'label': 'Layer 1'},
    2: {'linestyle': 'dashed', 'label': 'Layer 2'},
    3: {'linestyle': 'dotted', 'label': 'Layer 3'}
}

# === Plotting ===
fig, axes = plt.subplots(3, 3, figsize=(15, 12), sharex=True)
axes = axes.flatten()

for i, var in enumerate(variables):
    ax = axes[i]

    # --- Scalar variables (crop-specific) -
    if var in scalar_vars:
        if var in df1.columns:
            ax.plot(df1['Date'], df1[var],
                    color=dataset_colors['baseline'],
                    linestyle=scalar_linestyles['baseline'],
                    label=f'{var} (baseline)')

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

    # --- Layered/system-level variables grouped ---
    elif var in layered_var_groups:
        for j, layer_var in enumerate(layered_var_groups[var], start=1):
            style = layer_styles[j]

            if layer_var in df1.columns:
                ax.plot(df1['Date'], df1[layer_var],
                        color=dataset_colors['baseline'],
                        linestyle=style['linestyle'],
                        label=f'{layer_var} (baseline)')

            if layer_var in df2.columns:
                ax.plot(df2['Date'], df2[layer_var],
                        color=dataset_colors['intercropping'],
                        linestyle=style['linestyle'],
                        label=f'{layer_var} (intercropping)')

    else:
        ax.text(0.5, 0.5, f"{var} not found", ha='center', va='center')

    ax.set_title(var)
    ax.grid(True)
    ax.xaxis.set_major_formatter(mdates.DateFormatter('%b-%Y'))
    ax.tick_params(axis='x', rotation=45)

    # Clean duplicate legend entries
    handles, labels = ax.get_legend_handles_labels()
    by_label = dict(zip(labels, handles))
    ax.legend(by_label.values(), by_label.keys(), fontsize='small')

# === Final layout ===
plt.suptitle("Comparison: Single Crop vs. Intercropping (Daily Values)", fontsize=16)
plt.tight_layout(rect=[0, 0, 1, 0.97])
plt.show()
