let playerText = document.getElementById('playerText')
let restartBtn = document.getElementById('restartBtn')
let boxes = Array.from(document.getElementsByClassName('box'))

let winnerIndicator = getComputedStyle(document.body).getPropertyValue('--winning-blocks')

const O_TEXT = "O"
const X_TEXT = "X"
let currentPlayer = O_TEXT
let spaces = Array(9).fill(null)
let moves = 0;

const startGame = () => {
    boxes.forEach(box => box.addEventListener('click', boxClicked))
}

function computer() {
    if((moves % 2) === 1){
        var indexes = Array.from(Array(spaces.length).keys());
        var availableIndexes = indexes.filter((index) => spaces[index] == null);
        console.log(availableIndexes)
        var selectedIndex = availableIndexes[Math.floor(Math.random()* availableIndexes.length)];
        console.log(selectedIndex)
        if(availableIndexes.length === 0){
            playerText.innerHTML = 'It is a Tie!'
            setTimeout(restart(), 100);
            
        }
        spaces[selectedIndex] = currentPlayer
        document.getElementById(selectedIndex).innerText = currentPlayer
        moves = moves + 1
        if(playerHasWon() !==false){
            playerText.innerHTML = `${currentPlayer} has won!`
            let winning_blocks = playerHasWon()
    
            winning_blocks.map( box => boxes[box].style.backgroundColor=winnerIndicator)
        }
        currentPlayer = currentPlayer == X_TEXT ? O_TEXT : X_TEXT
        
    }
}

function boxClicked(e) {
    const id = e.target.id
    if(!spaces[id]){
        spaces[id] = currentPlayer
        e.target.innerText = currentPlayer
        moves = moves + 1
        if(playerHasWon() !==false){
            playerText.innerHTML = `${currentPlayer} has won!`
            let winning_blocks = playerHasWon()

            winning_blocks.map( box => boxes[box].style.backgroundColor=winnerIndicator)
            return
        }
        currentPlayer = currentPlayer == X_TEXT ? O_TEXT : X_TEXT
        computer()
    }
}

const winningCombos = [
    [0,1,2],
    [3,4,5],
    [6,7,8],
    [0,3,6],
    [1,4,7],
    [2,5,8],
    [0,4,8],
    [2,4,6]
]

function istie() {
    var availableIndexes = indexes.filter((index) => spaces[index] == null);
    if (availableIndexes.length === 0){
        return true
    } else {
        false
    }
}

function playerHasWon() {
    for (const condition of winningCombos) {
        let [a, b, c] = condition

        if(spaces[a] && (spaces[a] == spaces[b] && spaces[a] == spaces[c])) {
            return [a,b,c]
        }
    }
    return false
}

restartBtn.addEventListener('click', restart)

function restart() {
    spaces.fill(null)

    boxes.forEach( box => {
        box.innerText = ''
        box.style.backgroundColor=''
    })

    playerText.innerHTML = 'Tic Tac Toe'

    currentPlayer = X_TEXT
}

startGame()
