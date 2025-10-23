param(
    [string]$Port = 'COM10',
    [int]$Baud = 115200,
    [int]$ReadMs = 1200,
    [string]$SendChar = 'b'
)

function Test-ComPort {
    param(
        [string]$Port,
        [int]$Baud,
        [int]$ReadMs,
        [string]$SendChar
    )
    try {
        $sp = New-Object System.IO.Ports.SerialPort($Port, $Baud, [System.IO.Ports.Parity]::None, 8, [System.IO.Ports.StopBits]::One)
        $sp.ReadTimeout = 500
        $sp.WriteTimeout = 500
        $sp.Open()
        Start-Sleep -Milliseconds 200
        try { $sp.DiscardInBuffer() } catch {}
        if ($SendChar -and $SendChar.Length -gt 0) {
            try { $sp.Write($SendChar) } catch {}
        }
        Start-Sleep -Milliseconds $ReadMs
        $data = ''
        try { $data = $sp.ReadExisting() } catch {}
        $sp.Close()
        $snippet = if ($null -ne $data) { if ($data.Length -gt 800) { $data.Substring(0,800) } else { $data } } else { '' }
        Write-Output ("OK baud=${Baud} OPEN=true")
        if ($snippet) {
            Write-Output $snippet
        }
    } catch {
        Write-Output ("ERR baud=${Baud}: " + $_.Exception.Message)
    }
}

Write-Output ("Testing $Port @ $Baud")
Test-ComPort -Port $Port -Baud $Baud -ReadMs $ReadMs -SendChar $SendChar

# Also try a fallback at 9600 baud
if ($Baud -ne 9600) {
    Write-Output ("Testing $Port @ 9600")
    Test-ComPort -Port $Port -Baud 9600 -ReadMs $ReadMs -SendChar $SendChar
}

