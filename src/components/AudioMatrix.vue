<template>
  <div class="audio-matrix-panel">
    <div class="matrix-layout">
      <div class="matrix-corner"></div>

      <div class="column-labels">
        <div
          v-for="index in matrixSize"
          :key="`column-${index}`"
          class="matrix-label column-label"
        >
          {{ columnLabels[index - 1] || '' }}
        </div>
      </div>

      <div class="row-labels">
        <div
          v-for="index in matrixSize"
          :key="`row-${index}`"
          class="matrix-label row-label"
        >
          {{ rowLabels[index - 1] || '' }}
        </div>
      </div>

      <div class="matrix-grid">
        <template v-for="(row, rowIndex) in matrix">
          <button
            v-for="(active, columnIndex) in row"
            :key="`${rowIndex}-${columnIndex}`"
            type="button"
            class="matrix-cell"
            :class="{ 'is-active': active }"
            :aria-label="`第 ${rowIndex + 1} 行，第 ${columnIndex + 1} 列`"
            :aria-pressed="String(active)"
            @click="toggleCell(rowIndex, columnIndex)"
          ></button>
        </template>
      </div>
    </div>
  </div>
</template>

<script>
const MATRIX_SIZE = 8

export default {
  name: 'AudioMatrix',

  props: {
    value: {
      type: Array,
      default: () => []
    },
    columnLabels: {
      type: Array,
      default: () => ['', '', '', '', '', '', '', '']
    },
    rowLabels: {
      type: Array,
      default: () => ['', '', '', '', '', '', '', '']
    }
  },

  data() {
    return {
      matrixSize: MATRIX_SIZE,
      matrix: this.normalizeMatrix(this.value)
    }
  },

  watch: {
    value: {
      deep: true,
      handler(value) {
        this.matrix = this.normalizeMatrix(value)
      }
    }
  },

  methods: {
    normalizeMatrix(value) {
      return Array.from({ length: MATRIX_SIZE }, (_, rowIndex) =>
        Array.from(
          { length: MATRIX_SIZE },
          (_, columnIndex) => Boolean(value[rowIndex] && value[rowIndex][columnIndex])
        )
      )
    },

    toggleCell(rowIndex, columnIndex) {
      const nextMatrix = this.matrix.map(row => row.slice())
      nextMatrix[rowIndex][columnIndex] = !nextMatrix[rowIndex][columnIndex]
      this.matrix = nextMatrix

      this.$emit('input', nextMatrix)
      this.$emit('change', nextMatrix)
      this.$emit('cell-change', {
        row: rowIndex,
        column: columnIndex,
        active: nextMatrix[rowIndex][columnIndex]
      })
    }
  }
}
</script>

<style scoped>
.audio-matrix-panel {
  width: 500px;
  height: 500px;
  margin: auto;
  box-sizing: border-box;
  display: grid;
  place-items: center;
  overflow: hidden;
  background: #f2f3f5;
}

.matrix-layout {
  display: grid;
  grid-template-columns: 72px 400px;
  grid-template-rows: 58px 400px;
}

.matrix-corner {
  background: #30343a;
  border-right: 1px solid #4a4f56;
  border-bottom: 1px solid #4a4f56;
}

.column-labels {
  display: grid;
  grid-template-columns: repeat(8, 50px);
}

.row-labels {
  display: grid;
  grid-template-rows: repeat(8, 50px);
}

.matrix-label {
  box-sizing: border-box;
  display: flex;
  align-items: center;
  justify-content: center;
  overflow: hidden;
  color: #f5f7fa;
  background: #30343a;
  border-right: 1px solid #4a4f56;
  border-bottom: 1px solid #4a4f56;
  font-size: 12px;
  line-height: 1.2;
  text-align: center;
  word-break: break-all;
}

.column-label {
  padding: 4px 2px;
}

.row-label {
  padding: 2px 4px;
}

.matrix-grid {
  display: grid;
  grid-template-columns: repeat(8, 50px);
  grid-template-rows: repeat(8, 50px);
  border-top: 1px solid #aeb4bc;
  border-left: 1px solid #aeb4bc;
  box-sizing: border-box;
}

.matrix-cell {
  width: 50px;
  height: 50px;
  padding: 0;
  box-sizing: border-box;
  border: 0;
  border-right: 1px solid #aeb4bc;
  border-bottom: 1px solid #aeb4bc;
  outline: none;
  cursor: pointer;
  background: #ffffff;
  transition: background-color 0.12s ease;
}

.matrix-cell:hover {
  background: #eef6ff;
}

.matrix-cell:focus-visible {
  box-shadow: inset 0 0 0 2px #409eff;
}

.matrix-cell.is-active {
  background: #087cf0;
}

.matrix-cell.is-active:hover {
  background: #0672dc;
}
</style>
